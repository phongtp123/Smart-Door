/* implementation of binding file "custom,rc522.yaml" */
#define DT_DRV_COMPAT custom_rc522

#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include "rc522.h"

/* Register logging */
LOG_MODULE_REGISTER(rc522);

/***************************************************************************************************************
* THREADS DEFINITION
****************************************************************************************************************/
/* Define necessary threads structure */
static struct k_thread rc522_worker_thread;
static struct k_thread rc522_scan_thread;

/* Define stack size for threads */
#define RC522_WORKER_THREAD_STACK_SIZE      2048U
#define RC522_SCAN_THREAD_STACK_SIZE        2048U

/* Static define a toplevel kernel stack memory region for driver threads */
K_THREAD_STACK_DEFINE(rc522_worker_thread_stack, RC522_WORKER_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(rc522_scan_thread_stack, RC522_SCAN_THREAD_STACK_SIZE);

/* Threads priority (This driver have 2 threads with same priority) */
#define RC522_THREAD_PRIORITY   5U

/***************************************************************************************************************
* PROTOTYPE DECLARATIONS
****************************************************************************************************************/
static int rc522_init(const struct device* dev);
static void rc522_scan(const struct device* dev, const rc522_uid_cb_t cb);
static void rc522_getUID(const struct device* dev, uint8_t* rBuff,const uint8_t buffSize);
static void rc522_reset(const struct spi_dt_spec *spi);
static void rc522_setup_scan(const struct spi_dt_spec *spi, bool ATQA_request);
static void rc522_picc_request(const struct spi_dt_spec *spi, const uint8_t rf_cmd);
static void rc522_picc_select(const struct spi_dt_spec *spi);
static void rc522_receive_uid(const struct spi_dt_spec *spi, uint8_t *rBuff);
static void rc522_write_reg(const struct spi_dt_spec *spi, const uint8_t reg, const uint8_t* sBuff, const uint8_t buffSize);
static void rc522_set_reg_bitmask(const struct spi_dt_spec *spi, const uint8_t reg, const uint8_t mask);
static void rc522_read_reg(const struct spi_dt_spec* spi, const uint8_t reg, uint8_t* rBuff, const uint8_t buffSize);

/***************************************************************************************************************
* IMPLEMENTATION
****************************************************************************************************************/
/* ISR for handle IRQ pin, implemented simple for just make worker thread become active */
void rc522_irq_handler(const struct device *dev, struct gpio_callback *cb, gpio_port_pins_t pins)
{
    /* Retreat MFRC522 runtime data structure from reference to gpio_callback */
    struct rc522_data *data = CONTAINER_OF(cb, struct rc522_data, irq_gpio_cb);
    /* Give semaphore to worker thread */
    k_sem_give(&(data->irq_sem));
}

/* Implemented worker thread callback function */
void worker_thread_entry(void *p1, void *p2, void *p3)
{
    /* Cast p1 to RC522 data structure */
    struct rc522_data *data = (struct rc522_data*)p1;
    /* Cast p2 to RC522 config structure */
    const struct rc522_config *config = (const struct rc522_config*)p2;
    /* Get SPI module spec */
    const struct spi_dt_spec *spi = &(config->spi);
    /* Buffer store general MFRC522 register for individual read */
    uint8_t buffer = 0u;
    /* Dummy variable store command/data for individual write */
    uint8_t dummy = 0u;
    
    /* Start thread function */
    while (1)
    {
        /* Need a semaphore available to execute */
        k_sem_take(&(data->irq_sem), K_FOREVER);
        /* Disable RxIRq, TimerIRq and ErrorIRq interrupt trigger on MFRC522(advoid overlap trigger) */
        dummy = 0x80;
        /* Send command */
        rc522_write_reg(spi, RC522_COM_IRQ_EN_REG, &dummy, RC522_REG_SIZE_BYTE);
        /* Read IRQ Com Flags from ComIrqReg */
        rc522_read_reg(spi, RC522_COM_IRQ_REG, &buffer, RC522_REG_SIZE_BYTE);
        /* After each command is excecuted by the MFRC522, if IRQ of the command is enable, the signal on pin IRQ of
        * the MFRC522 will be assered and trigger IRQ pin if the host that control the MFRC522 to generate an IRQ and
        * this function will be called to handle that IRQ. There are 3 IRQ flags of the MFRC522 that should be considered
        * when IRQ pin is trigger IRQ : RxIRq, ErrIRq and TimerIRq. To get IRQ flags bit mask, host need to read ComIrqReg
        * register of the MFRC522. Check IRQ flag after read IRQ status register as order: RxIRq->TimerIRq + ErrIRq for best
        * efficiency.  */
        if(RC522_GET_IRQ_BIT(buffer, RC522_RX_IRQ_BIT))
        {
            /* First check the FIFO Buffer size(in FIFOLevelReg) */
            rc522_read_reg(spi, RC522_FIFO_LEVEL_REG, &buffer, RC522_REG_SIZE_BYTE);
            if(buffer == RC522_ATQA_BYTES_NUM)
            {
                /* FIFO Level respond 2 byte are in the FIFO Buffer and that is the size of the ATQA respond packet, process
                * to write ANTICOLLISION command to FIFO Buffer and wait for next RxIRq flag */
                /* Set up new scan for UID after noticed received ATQA message */
                rc522_setup_scan(spi, false);
                /* Start transmission */
                rc522_picc_select(spi);
            }
            else if(buffer == (RC522_CL_1_BYTES_NUM + 1U))
            {
                /* RC522_CL_1_BYTES_NUM + 1U mean in FIFO Buffer currently store 4 byte of UID and 1 byte of BCC(Byte Character 
                 * Check)
                * process to read data from FIFO, make BCC check and give controller to intialize new scan */
                rc522_receive_uid(spi, data->uid);
                /* If there is callback function then call it before start new scan */
                if(data->card_detected_cb != NULL)
                {
                    data->card_detected_cb(data->uid, RC522_CL_1_BYTES_NUM);
                }
                /* Small delay before start new scan */
                k_msleep(100);
                /* Give semaphore to make new scan thread */
                k_sem_give(&(data->scan_sem));
            }
        }
        /* Handle Timer IRQ flag and Error IRQ flag */
        else if(RC522_GET_IRQ_BIT(buffer, (RC522_TIMER_IRQ_BIT | RC522_ERR_IRQ_BIT)))
        {
            /* Small delay before start new scan */
            k_msleep(100);
            /* Give semaphore to make new scan thread */
            k_sem_give(&(data->scan_sem));
        }
        /* Unpredicted IRQ flag as no enable to trigger interrupt on MFRC522 but still propagated to IRQ pin */
        else
        {
            /* Enable interrupt trigger on MFRC522 */
            dummy = RC522_EN_IRQ_CMD(RC522_EN_RX_IRQ_MASK | RC522_EN_ERR_IRQ_MASK | RC522_EN_TMR_IRQ_MASK);
            /* Send command to MFRC522 */
            rc522_write_reg(spi, RC522_COM_IRQ_EN_REG, &dummy, RC522_REG_SIZE_BYTE);
        }
    }
}

/* Implemented scan thread callback function */
void scan_thread_entry(void *p1, void *p2, void *p3)
{
    /* Cast p1 to RC522 data structure */
    struct rc522_data *data = (struct rc522_data*)p1;
    /* Cast p2 to RC522 config structure */
    const struct rc522_config *config = (const struct rc522_config*)p2;
    /* Get SPI module spec */
    const struct spi_dt_spec *spi = &(config->spi);

    /* Start scan thread entry */
    while(1)
    {
        /* Execute thread fucntions when there is semaphore available */
        k_sem_take(&(data->scan_sem), K_FOREVER);
        /* Set up registers on MFRC522 before start scan */
        rc522_setup_scan(spi, true);
        /* Start PICC request for ATQA message */
        rc522_picc_request(spi, PICC_CMD_REQA);
    }
}

static int rc522_init(const struct device* dev)
{
    /* Error code, expect equal to 0 for process OK */
    int ret = 0u;
    /* Cast config */
    const struct rc522_config* config = (const struct rc522_config*)dev->config;
    /* Cast runtime data */
    struct rc522_data* data = (struct rc522_data*)dev->data;
    /* Get SPI config */
    const struct spi_dt_spec* spi = &(config->spi);
    /* Get RC522 IRQ pin if host control RC522 in IRQ mode */
    const struct gpio_dt_spec* irq_gpio = &(config->irq_pin);
    /* Array of necessary registers for init phase */
    const uint8_t init_reg[] = {
        RC522_TIMER_MODE_REG,
        RC522_TIMER_PRESCALER_REG,
        RC522_TIMER_RELOAD_HI_REG,
        RC522_TIMER_RELOAD_LOW_REG,
        RC522_TX_ASK_REG,
        RC522_RX_MODE_REG,
        RC522_RF_CONFIG_REG,
        RC522_TX_CTRL_REG,
        RC522_DIV_IRQ_EN_REG
    };
    /* Array of necessary commands for init phase */
    const uint8_t init_cmd[] = {
        /**
         * When communicating with a PICC we need a timeout if something goes wrong.
         * f_timer = 13.56 MHz / ((2*TPreScaler + 1) * (TReloadVal + 1)) where TPreScaler = [TPrescaler_Hi:TPrescaler_Lo]
         * TPrescaler_Hi are the four low bits[3:0] in TModeReg. TPrescaler_Lo is in TPrescalerReg.
         * TReloadVal_Hi and TReloadVal_Lo is in TReloadReg 
         * 
         * - 0x80 : Set timer in automatically mode (TAuto=1), timer starts automatically at the end of the transmission in all 
         *          communication modes at all speeds
         * - 0xA9 : Set prescaler low 8 bit value in TPrescalerReg (Prescaler high 4 bit by default is set to 0x0 after reset)
         *          is 0xA9 = 169 => f_timer = 40kHz, timer period of 25us
         * - 0x03 : Set timer reload value is 1000, that mean 25ms before timer timeout. TReloadVal_Hi=0x03
         * - 0xE8 : Set timer reload value is 1000, that mean 25ms before timer timeout. TReloadVal_Lo=0xE8*/
        0x80, 0xA9, 0x03, 0xE8,
        /* Force a 100% ASK modulation independent of the ModGsPReg register setting, reference to MFRC522 datasheet page49 */
        0x40,
        /* Enable Receiver no error functional */
        RC522_ENABLE_RX_NO_ERR,
        /* Set MFRC522 receiver signal voltage gain factor to average 33dB gain */
        RC522_RXGAIN_AVG,
        /* Enable the antenna driver pins TX1 and TX2 (they were disabled by the reset) */
        RC522_ENABLE_TX1_TX2,
        /* Set IRq pin as standard CMOS pin */
        0x80,
    };
    /* Version of MFRC522 software, use for verify SPI communicate with device */
    uint8_t version = 0u;
    /* Iterate variable */
    uint8_t i = 0u;

    /* Start initializing MFRC522 device */
    LOG_INF("**Initializing MFRC522 (device ID: %u)**\r\n", config->id);
    /* Initialize driver semaphore */
    ret = k_sem_init(&(data->irq_sem), 0u, 1u);
    if(ret)
    {
        LOG_ERR("Cannot initialize IRQ semaphore\r\n");
    }
    ret = k_sem_init(&(data->scan_sem), 0u, 1u);
    if(ret)
    {
        LOG_ERR("Cannot initialize Scan semaphore\r\n");
    }
    /* Check SPI device is ready */
    if(spi_is_ready_dt(spi))
    {
        /* Read version */
        rc522_read_reg(spi, RC522_VERSION_REG, &version, RC522_REG_SIZE_BYTE);
        /* Log version */
        LOG_INF("**MFRC522 software version = 0x%02X**\r\n", version);
        /* Reset MFRC522 */
        rc522_reset(spi);
        /* Send commands to their corresponding registers */
        for(i = 0u ; i < ARRAY_SIZE(init_reg); i++)
        {
            rc522_write_reg(spi, init_reg[i], &init_cmd[i], RC522_REG_SIZE_BYTE);
        }
    }
    else
    {
        /* SPI is not ready */
        LOG_ERR("SPI device is not ready as some error occur!\r\n");
        ret = -ENODEV;
    }
    /* Check if IRQ GPIO is ready */
    if(gpio_is_ready_dt(irq_gpio))
    {
        /* Set IRQ pin as input */
        ret = gpio_pin_configure_dt(irq_gpio, GPIO_INPUT);
        if(!ret)
        {
            /* Configure to trigger the interrupt when the IRQ pin is asserted */
            ret = gpio_pin_interrupt_configure_dt(irq_gpio, GPIO_INT_EDGE_FALLING);
            if(!ret)
            {
                /* Connect ISR handler to IRQ trigger source */
                gpio_init_callback(&(data->irq_gpio_cb), rc522_irq_handler, BIT(irq_gpio->pin));
                gpio_add_callback_dt(irq_gpio, &(data->irq_gpio_cb));
            }
            else
            {
                LOG_ERR("Could not configure IRQ pin as interrupt source\r\n");
            }
        }
        else
        {
            LOG_ERR("Could not set IRQ pin as input\r\n");
        }
    }
    else
    {
        /* GPIO is not ready, return error log and error code */
        LOG_ERR("IRQ pin communicate with RC522 is not ready as some error occur!\r\n");
        ret = -ENODEV;
    }
    if(!ret)
    {
        LOG_INF("MFRC522 initialize successfully.\r\n");
    }

    return ret;
}

static void rc522_reset(const struct spi_dt_spec *spi)
{
    /* Dummy variable store soft reset command write to MFRC522 */
    const uint8_t dummy = RC522_RESET_CMD;
    /* Counter value for wait round till reset is completed */
    uint8_t count = 0u;
    /* Command Reg receive buffer */
    uint8_t cmdBuffer = 0u;

    /* Issue the soft reset command */
    rc522_write_reg(spi, RC522_CMD_REG, &dummy, RC522_REG_SIZE_BYTE);
    /* The datasheet does not mention how long the SoftRest command takes to complete. But the MFRC522 might have been in 
    * soft power-down mode (triggered by bit 4 of CommandReg). So let wait until bit PowerDown in CommandReg is clear, max 
    * 3x50 ms */
    do
    {
        /* Read CommandReg and store is cmdBuffer */
        rc522_read_reg(spi, RC522_CMD_REG, &cmdBuffer, RC522_REG_SIZE_BYTE);
        /* Increase counter (max 3) */
        count++;
        /* Process to delay the reset function to return */
        k_msleep(50);
    } while ((cmdBuffer & BIT(4)) && (count < 3u));
}

static void rc522_setup_scan(const struct spi_dt_spec *spi, bool ATQA_request)
{
    /* If at ATQA request phase, send command last byte 7-bit frame oriented to MFRC522 as REQA/WUPA is 7-bit long. If not then send 
     * command to set last byte 8-bit frame oriented */
    uint8_t dummy = ATQA_request ? RC522_RF_BIT_FRAME : RC522_NORMAL_BIT_FRAME;
    /* Array of registers necessary to setup scan progress */
    const uint8_t setupReg[] = { 
        RC522_CMD_REG,
        RC522_FIFO_LEVEL_REG,
        RC522_BIT_FRAME_REG,
        RC522_COM_IRQ_REG,
    };
    /* Array of commands/data for these registers */
    const uint8_t setupCMD[] = { 
        RC522_IDLE_CMD,
        RC522_RESET_FIFO_CMD,
        dummy,
        RC522_CLEAR_ALL_IRQ_BIT,
    };
    /* Iteration variable */
    uint8_t i = 0u;

    /* Transmit commands to MFRC522 */
    for(i = 0u; i < ARRAY_SIZE(setupReg); i++)
    {
        rc522_write_reg(spi, setupReg[i], &setupCMD[i], RC522_REG_SIZE_BYTE);
    }
}

static void rc522_scan(const struct device* dev, const rc522_uid_cb_t cb)
{
    /* Cast config */
    struct rc522_config *config = (struct rc522_config*)dev->config;
    /* Cast data */
    struct rc522_data *data = (struct rc522_data*)dev->data;
    
    /* Save callback function to runtime data if available */
    if(cb != NULL)
    {
        data->card_detected_cb = cb;
    }

    /* Create threads */
    k_thread_create(
        &rc522_worker_thread, 
        rc522_worker_thread_stack, 
        K_THREAD_STACK_SIZEOF(rc522_worker_thread_stack),
        worker_thread_entry,
        data, config, NULL,
        RC522_THREAD_PRIORITY, 0, K_NO_WAIT
    );
    k_thread_create(
        &rc522_scan_thread, 
        rc522_scan_thread_stack, 
        K_THREAD_STACK_SIZEOF(rc522_scan_thread_stack),
        scan_thread_entry,
        data, config, NULL,
        RC522_THREAD_PRIORITY, 0, K_NO_WAIT
    );
    
    /* Give semaphore to scan thread as starting scan phase */
    k_sem_give(&(data->scan_sem));
}

static void rc522_getUID(const struct device* dev, uint8_t* rBuff,const uint8_t buffSize)
{
    /* Get MFRC522 runtime data structure */
    struct rc522_data *data = (struct rc522_data*)dev->data;

    /* Buffer size need at least equal to Cascade Level 1 UID size */
    if(buffSize < RC522_CL_1_BYTES_NUM)
    {
        LOG_ERR("Unsufficient buffer, expected %u bytes but %u bytes is provided\r\n", RC522_CL_1_BYTES_NUM, buffSize);
    }
    else
    {
        memcpy(rBuff, data->uid, RC522_CL_1_BYTES_NUM);
    }
}

static void rc522_picc_request(const struct spi_dt_spec *spi, const uint8_t rf_cmd)
{
    /* Array of registers necessary to request PICC respond */
    const uint8_t reg[] = { 
        RC522_FIFO_DATA_REG,
        RC522_CMD_REG,
        RC522_COM_IRQ_EN_REG,
    };
    /* Array of commands/data for these registers */
    const uint8_t cmd[] = { 
        rf_cmd,
        RC522_TRANSCEIVE_CMD,
        RC522_EN_IRQ_CMD(RC522_EN_RX_IRQ_MASK | RC522_EN_ERR_IRQ_MASK | RC522_EN_TMR_IRQ_MASK),
    };
    /* Iterate variable */
    uint8_t i = 0u;

    /* Transmit commands to MFRC522 */
    for(i = 0u; i < ARRAY_SIZE(reg); i++)
    {
        rc522_write_reg(spi, reg[i], &cmd[i], RC522_REG_SIZE_BYTE);
    }
    /* Start transmission */
    rc522_set_reg_bitmask(spi, RC522_BIT_FRAME_REG, RC522_TRANSCEIVE_START_SEND);
}

static void rc522_picc_select(const struct spi_dt_spec *spi)
{
    /* Array of SELECT/ANTICOLLISION commands need to write to MFRC522 FIFO Buffer register */
    const uint8_t sel_cmd[] = {
        PICC_CMD_SEL_CL1,
        PICC_SEND_BITS_NUM
    };
    /* Dummy variable store commands to send individually */
    uint8_t dummy = 0u;

    /* Write anticollision/select command and number of bits to receive from PICC into FIFO Buffer register 
     * before start select */
    rc522_write_reg(spi, RC522_FIFO_DATA_REG, sel_cmd, ARRAY_SIZE(sel_cmd));
    /* Set MFRC522 in Transceive mdoe */
    dummy = RC522_TRANSCEIVE_CMD;
    /* Send to MFRC522 */
    rc522_write_reg(spi, RC522_CMD_REG, &dummy, RC522_REG_SIZE_BYTE);
    /* Enable interrupt trigger on MFRC522 */
    dummy = RC522_EN_IRQ_CMD(RC522_EN_RX_IRQ_MASK | RC522_EN_ERR_IRQ_MASK | RC522_EN_TMR_IRQ_MASK);
    /* Send to MFRC522 */
    rc522_write_reg(spi, RC522_COM_IRQ_EN_REG, &dummy, RC522_REG_SIZE_BYTE);
    /* Start transmission */
    rc522_set_reg_bitmask(spi, RC522_BIT_FRAME_REG, RC522_TRANSCEIVE_START_SEND);
}

static void rc522_receive_uid(const struct spi_dt_spec *spi, uint8_t *rBuff)
{
    /* Buffer store UID + BCC, UID have 4 bytes long and BCC is 1 byte */
    uint8_t uid[RC522_CL_1_BYTES_NUM + 1u];
    /* Manual calculate BCC store in this variable */
    uint8_t bcc = 0u;

    /* Read UID + BCC */
    rc522_read_reg(spi, RC522_FIFO_DATA_REG, uid, RC522_CL_1_BYTES_NUM + 1u);
    /* Calculate BCC by XOR each of UID byte together */
    bcc = uid[0] ^ uid[1] ^ uid[2] ^ uid[3];
    /* Check BCC calculated and BCC received to make BCC check */
    if(bcc != uid[4])
    {
        LOG_ERR("BCC checked failed, expected %u but %u is received\r\n", bcc, uid[4]);
    }
    else
    {
        /* Pass BCC, process save to receive buffer */
        memcpy(rBuff, uid, RC522_CL_1_BYTES_NUM);
    }
}

static void rc522_write_reg(const struct spi_dt_spec *spi, const uint8_t reg, const uint8_t* sBuff, const uint8_t buffSize) {
    /* Construct byte 0, write mode and register on MFRC522 to write */
    const uint8_t addr_byte = RC522_ADDRESS_BYTE_FORMATTER(RC522_WRITE_BIT, reg);
    /* Construct SPI buffer, according to MFRC522 datasheet, It is possible to write up to n data bytes by only sending one 
     * address byte  */
    const struct spi_buf tx_buf[2u] = {
        { &addr_byte, 1u },
        { sBuff, buffSize }
    };
    /* Construct SPI buffer set (required to used SPI module APIs), send n SPI buffer to MOSI line */
    const struct spi_buf_set tx = {
        .buffers = tx_buf,
        .count = 2u,
    };

    spi_write_dt(spi, &tx);
}

static void rc522_set_reg_bitmask(const struct spi_dt_spec *spi, const uint8_t reg, const uint8_t mask)
{
    /* Receive buffer store data received from MFRC522 */
    uint8_t buffer = 0u;

    /* Read register value */
    rc522_read_reg(spi, reg, &buffer, RC522_REG_SIZE_BYTE);
    /* OR with bitmask if receive data process is success */
    buffer |= mask;
    /* Set register value after OR with bit mask */
    rc522_write_reg(spi, reg, &buffer, RC522_REG_SIZE_BYTE);
}

static void rc522_read_reg(const struct spi_dt_spec* spi, const uint8_t reg, uint8_t* rBuff, const uint8_t buffSize)
{
    uint8_t tx_data[buffSize + 1];
    uint8_t rx_data[buffSize + 1];

    /* According to MFRC522 datasheet: Repeat address byte N times, end with 0x00 byte */
    uint8_t addr_byte = RC522_ADDRESS_BYTE_FORMATTER(RC522_READ_BIT, reg);
    for (int i = 0; i < buffSize; i++) {
        tx_data[i] = addr_byte;
    }
    /* Last byte is 0x00 to pull CS line for last receive data */
    tx_data[buffSize] = 0x00;

    const struct spi_buf tx_buf = { 
        .buf = tx_data,
        .len = buffSize + 1
    };
    const struct spi_buf_set tx  = { 
        .buffers = &tx_buf,
        .count = 1u
    };
    const struct spi_buf rx_buf = { 
        .buf = rx_data,
        .len = buffSize + 1
    };
    const struct spi_buf_set rx  = { 
        .buffers = &rx_buf,
        .count = 1u
    };

    spi_transceive_dt(spi, &tx, &rx);
    memcpy(rBuff, &rx_data[1], buffSize);
}

/***************************************************************************************************************
* DEVICE TREE HANDLING
****************************************************************************************************************/
/* Construct MFRC522 API structure (assign struct with static keyword as not public this struct for other module
except for this module, these API public with device node of "custom,rc522" compatible) */
static const struct rc522_api rc522_api_funcs = {
    .startScanning = rc522_scan,
    .getUID        = rc522_getUID,
};

static const spi_operation_t spi_operation = (SPI_OP_MODE_MASTER | 
                                              SPI_TRANSFER_MSB |
                                              SPI_WORD_SET(8) |
                                              SPI_LINES_SINGLE);

/* Automate construct rc522 config when a MFRC522 device instance is created */
#define RC522_CONFIG_INST(inst)                                     \
    static const struct rc522_config rc522_config_##inst = {        \
        .spi = SPI_DT_SPEC_INST_GET(inst, spi_operation, 0u),       \
        .irq_pin = GPIO_DT_SPEC_INST_GET(inst, irq_gpios),          \
        .id = inst,                                                 \
    };

/* Automate construct rc522 runtime data when a MFRC522 device instance is created, init and run ok. This runtime data is existed 
 * for the rest of device life */
#define RC522_DATA_INST(inst)                                       \
    static struct rc522_data rc522_data_##inst;

/* Initialize device driver for MFRC522 when a MFRC522 device instance is created */
#define RC522_DEVICE_INST(inst)                                     \
    DEVICE_DT_INST_DEFINE(inst,                                     \
                          rc522_init,                               \
                          NULL,                                     \
                          &rc522_data_##inst,                       \
                          &rc522_config_##inst,                     \
                          POST_KERNEL,                              \
                          CONFIG_SPI_INIT_PRIORITY,                 \
                          &rc522_api_funcs);

/* Function macro that construct config and initialize device driver for BH1750 at booting time  */
#define RC522_INST(inst)       \
    RC522_CONFIG_INST(inst)    \
    RC522_DATA_INST(inst)      \
    RC522_DEVICE_INST(inst)
/* Each bh1750 instance is created in device tree will call BH1750_INST with different instance number */
DT_INST_FOREACH_STATUS_OKAY(RC522_INST)
