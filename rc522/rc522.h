#ifndef ZEPHYR_DRIVERS_RC522_H_
#define ZEPHYR_DRIVERS_RC522_H_

#include <errno.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>

/** MFRC522 General define **/
#define RC522_REG_SIZE_BYTE     (1U)    /* Mostly MFRC522 registers have size 8-bit, except for FIFO data register */
#define RC522_WRITE_BIT         (0U)    /* If this bit is present in address byte then MFRC522 run in receiver mode */
#define RC522_READ_BIT          (1U)    /* If this bit is present in address byte then MFRC522 run in transmitter mode */

/* MFRC522 address byte 0 frame format
* - [7] MSB bit : Define the mode used (write to register or read from register).
* - [6:1] Register address.
* - [0] LSB bit : Fixed value 0 
*/
#define RC522_OPERATION_MODE_SHIFT  (7U)
#define RC522_OPERATION_MODE_MASK   (0x80U)
#define RC522_ADDRESS_SHIFT         (1U)
#define RC522_ADDRESS_MASK          (0x7EU)
#define RC522_ADDRESS_BYTE_FORMATTER(modeBit, addr) \
    (uint8_t)((uint8_t)((modeBit << RC522_OPERATION_MODE_SHIFT) & RC522_OPERATION_MODE_MASK) | \
              (uint8_t)((addr << RC522_ADDRESS_SHIFT) & RC522_ADDRESS_MASK))

/* ISO/IEC 14443-A request commands */
#define PICC_CMD_REQA      (0x26U)
#define PICC_CMD_WUPA      (0x52U)

/* ISO/IEC 14443-A Anticollision/Select PICCs commands */
#define PICC_CMD_SEL_CL1   (0x93U)

/* Number of valid bits receive from PICC after selected */
#define PICC_SEND_BITS_NUM  (0x20U)

/* ATQA packet bytes num */
#define RC522_ATQA_BYTES_NUM    (2U)

/* Cascade Level 1 byte length */
#define RC522_CL_1_BYTES_NUM    (4U)

/** MFRC522 register addresses **/
/*-----------------------------------------------------------------------------------------------------
* PAGE 0: TRANSFER COMMAND AND STATUS REGISTER ADDRESSES
*------------------------------------------------------------------------------------------------------
* 0x01 : CommandReg - Start and stop command execution
* 0x02 : ComIEnReg - Enable and disable interrupt request control bits(FIFO buffer status interrupts)
* 0x03 : DivIEnReg - Enable and disable interrupt request control bits(CRC status interrupts)
* 0x04 : ComIrqReg - Interrupt request bits(FIFO buffer status interrupts)
* 0x05 : DivIrqReg - Interrupt request bits(CRC buffer status interrupts)
* 0x06 : ErrorReg - Error bits showing the error status of the last command executed
* 0x07 : Status1Reg - Communication status bits
* 0x08 : Status2Reg - Receiver and transmitter status bits
* 0x09 : FIFODataReg - Input and output of 64 bytes FIFO buffer
* 0x0A : FIFOLevelReg - Number of bytes stored in the FIFO bufer
* 0x0B : WaterLevelReg - Level of FIFO underflow and overflow warning to be triggered
* 0x0C : ControlReg - Miscellaneous control registers
* 0x0D : BitFramingReg - Adjustments for bit-oriented frames register
* 0x0E : CollReg - bit position of the first bit-collision detected on the RF interface */
#define RC522_CMD_REG              0x01
#define RC522_COM_IRQ_EN_REG       0x02
#define RC522_DIV_IRQ_EN_REG       0x03
#define RC522_COM_IRQ_REG          0x04
#define RC522_DIV_IRQ_REG          0x05
#define RC522_ERR_STATUS_REG       0x06
#define RC522_STATUS_1_REG         0x07
#define RC522_STATUS_2_REG         0x08
#define RC522_FIFO_DATA_REG        0x09
#define RC522_FIFO_LEVEL_REG       0x0A
#define RC522_FIFO_WATERMARK_REG   0x0B
#define RC522_CTRL_REG             0x0C
#define RC522_BIT_FRAME_REG        0x0D
#define RC522_COLL_REG             0x0E

/*-------------------------------------------------------------------------------------------------------
* PAGE 1: TRANSFER CONFIGURATION COMMAND REGISTER ADDRESSES
*-------------------------------------------------------------------------------------------------------
* 0x11 : ModeReg - Define general modes for transmitting and receiving
* 0x12 : TxModeReg - Define transmission data rate and framing
* 0x13 : RxModeReg - Define reception data rate and framing
* 0x14 : TxControlReg - Controls the logical behavior of the antenna driver pins TX1 and TX2
* 0x15 : TxASKReg - Controls the setting of the transmission modulation
* 0x16 : TxSelReg - Select the internal sources for the antenna driver
* 0x17 : RxSelReg - Select internal receiver settings
* 0x18 : RxThresholdReg - Select threshold for the bit decoder
* 0x19 : DemodReg - Define demodulator settings
* 0x1C : MfTxReg - Control some MIFARE Communication transmit parameters
* 0x1D : MfRxReg - Control some MIFARE Communication receive parameters
* 0x1F : SerialSpeedReg - Select the speed of the serial UART interface */
#define RC522_GENERAL_MODE_REG      0x11
#define RC522_TX_MODE_REG           0x12
#define RC522_RX_MODE_REG           0x13
#define RC522_TX_CTRL_REG           0x14
#define RC522_TX_ASK_REG            0x15
#define RC522_TX_SEL_REG            0x16
#define RC522_RX_SEL_REG            0x17
#define RC522_RX_THRESH_DECODE_REG  0x18
#define RC522_DEMOD_REG             0x19
#define RC522_MIFARE_TX_REG         0x1C
#define RC522_MIFARE_RX_REG         0x1D
#define RC522_SERIAL_SPEED_REG      0x1F

/*-------------------------------------------------------------------------------------------------------
* PAGE 2: CRC, TIMER, INTERNAL CONFIGURATION REGISTER ADDRESSES
*-------------------------------------------------------------------------------------------------------
* 0x21 , 0x22 : CRCResultReg - Show the MSB and LSB values of the CRC calculation
* 0x24 : ModWidthReg - Control the ModWidth settings
* 0x26 : RFCfgReg - Configure the RF receiver gain
* 0x27 : GsNReg - Select the conductance of the antenna driver pins TX1 and TX2 for modulation
* 0x28 : CWGsPReg - Define the conductance of the p-driver output during periods of no modulation
* 0x29 : ModGsPReg - Define the conductance of the p-driver output during periods of modulation
* 0x2A : TModeReg - Configure settings for the internal timer
* 0x2B : TPrescalerReg - Configure settings for the internal timer
* 0x2C, 0x2D : TReloadReg - Define the 16-bit timer reload value
* 0x2E, 0x2F : TCounterValReg - Show the 16-bit timer value  */
#define RC522_CRC_RES_MSB_REG       0x21
#define RC522_CRC_RES_LSB_REG       0x22
#define RC522_MOD_WIDTH_REG         0x24
#define RC522_RF_CONFIG_REG         0x26
#define RC522_GSN_REG               0x27
#define RC522_NOMOD_GSP_REG         0x28
#define RC522_MOD_GSP_REG           0x29
#define RC522_TIMER_MODE_REG        0x2A
#define RC522_TIMER_PRESCALER_REG   0x2B
#define RC522_TIMER_RELOAD_HI_REG   0x2C
#define RC522_TIMER_RELOAD_LOW_REG  0x2D
#define RC522_TIMER_CNTVAL_HI_REG   0x2E
#define RC522_TIMER_CNTVAL_LOW_REG  0x2F

/*-------------------------------------------------------------------------------------------------------
* PAGE 3: TEST USE CASE REGISTER ADDRESSES
*------------------------------------------------------------------------------------------------------- 
* 0x31 : TestSel1Reg - general test signal configuration
* 0x32 : TestSel2Reg - general test signal configuration and PRBS control
* 0x33 : TestPinEnReg - enables pin output driver on pins D1 to D7
* 0x34 : TestPinValueReg - defines the values for D1 to D7 when it is used as an I/O bus
* 0x35 : TestBusReg - shows the status of the internal test bus
* 0x36 : AutoTestReg - controls the digital self test
* 0x37 : VersionReg - shows the software version
* 0x38 : AnalogTestReg - controls the pins AUX1 and AUX2
* 0x39 : TestDAC1Reg - defines the test value for TestDAC1
* 0x3A : TestDAC2Reg - defines the test value for TestDAC2
* 0x3B : TestADCReg - shows the value of ADC I and Q channels
* 0x3C - 0x3F : Reserved */
#define RC522_TEST_CFG_1_REG        0x31
#define RC522_TEST_CFG_2_REG        0x32
#define RC522_TEST_PIN_EN_REG       0x33
#define RC522_TEST_PIN_VAL_REG      0x34
#define RC522_TEST_BUS_REG          0x35
#define RC522_TEST_AUTO_REG         0x36
#define RC522_VERSION_REG           0x37
#define RC522_TEST_ANALOG_REG       0x38
#define RC522_TEST_DAC_1_REG        0x39
#define RC522_TEST_DAC_2_REG        0x3A
#define RC522_TEST_ADC_REG          0x3B

/* 8-bit CommandReg (Reset = 0x20u)
* - [7:4] Fixed value : 0010
* - [3:0] Command : Activate a command based on Command value
*    + 0000 - Idle command: No action, cancels current command execution.
*    + 0001 - Mem command: Stores 25 bytes into the internal buffer.
*    + 0010 - Generate RandomID command: Generate a random 10-byte ID number.
*    + 0011 - CalcCRC command: Activate a CRC coprocessor or perform a self test.
*    + 0100 - Transmit command: Transmit data from FIFO buffer.
*    + 0111 - No CMD Change command: Can be used to modify the CommandReg register bits without affecting the current executed *    command
*    + 1000 - Receive command: Activate the receiver circuits.
*    + 1100 - Transceive command: Transmit data from FIFO buffer to antenna and automatically activates the receiver after
*    transmission.
*    + 1110 - MIFARE Authenticate command: Perform the MIFARE Standard authentication as a reader.
*    + 1111 - Soft Reset command: Resets the MFRC522. */
#define RC522_IDLE_CMD          (0x00U)
#define RC522_MEM_CMD           (0x01U)
#define RC522_GEN_RAND_ID_CMD   (0x02U)
#define RC522_CALC_CRC_CMD      (0x03U)
#define RC522_TRANSMIT_CMD      (0x04U)
#define RC522_NO_CMD_CHANGE     (0x07U)
#define RC522_RECEIVE_CMD       (0x08U)
#define RC522_TRANSCEIVE_CMD    (0x0CU)
#define RC522_MF_AUTH_CMD       (0x0EU)
#define RC522_RESET_CMD         (0x0FU)

/* 8-bit FIFOLevelReg (reset = 0x00)
* - [7] FlushBuffer (Write-only) : immediately clears the internal FIFO buffer’s and ErrorReg register’s BufferOvfl bit.
* - [6:0] FIFOLevel : indicates the number of bytes stored in the FIFO buffer. */
#define RC522_RESET_FIFO_CMD    (0x80)

/* 8-bit TxControlReg (Reset = 0x80) 
* - [7:2] Fixed value: 100000
* - [1] Tx2RFEn : Set to 1 to enable output signal on pin TX2 delivers the 13.56 MHz energy carrier modulated by the transmission 
*   data
* - [0] Tx1RFEn : Set to 1 to enable output signal on pin TX1 delivers the 13.56 MHz energy carrier modulated by the transmission 
*   data 
*/
#define RC522_ENABLE_TX1_TX2    (0x83U)

/* 8-bit RxModeReg (Reset = 0x00)
* - [7:4] Default reset value: 0000
* - [3] RxNoErr : Set to 1 to avoid received invalid data stream (less than 4-bit received) and the receiver remains active
* - [2:0] Default reset value: 000 */
#define RC522_ENABLE_RX_NO_ERR  (0x08U)

/* 8-bit BitFramingReg (Reset = 0x00)
* - [7] StartSend : starts the transmission of data only valid in combination with the <Transceive> command
* - [6:3] Default reset value: 0000
* - [2:0] TxLastBits : Define the number of bits of the last byte that will be transmitted.
*   + 111 : 7 bits will be transmitted (dedicated for transmit REQA or WUPA packet to antenna TX1, TX2)
*   + 000 : All byte will be transmitted (dedicated for normal transmit data back to host) */
#define RC522_RF_BIT_FRAME          (0x07)
#define RC522_NORMAL_BIT_FRAME      (0x00)
#define RC522_TRANSCEIVE_START_SEND (0x80)

/* 8-bit RFCfgReg (reset = 0x48)
* - [7] and [3:0] : Reserved
* - [6:4] RxGain : defines the receiver’s signal voltage gain factor.
*   + 000 : 18dB (min)
*   + 001 : 23dB
*   + 010 : 18dB
*   + 011 : 23dB
*   + 100 : 33dB (avg)
*   + 101 : 38dB
*   + 110 : 43dB
*   + 111 : 48dB (max) 
*/
#define RC522_RFCFG_RXGAIN_SHIFT    (4U)
#define RC522_RXGAIN_MIN            (0x00 << RC522_RFCFG_RXGAIN_SHIFT)
#define RC522_RXGAIN_AVG            (0x04 << RC522_RFCFG_RXGAIN_SHIFT)
#define RC522_RXGAIN_MAX            (0x07 << RC522_RFCFG_RXGAIN_SHIFT)

/* MFRC522 ComIrqReg
* - [7] Set1 : Set this bit to make this register in "write-2-clear" mode
* - [6] TxIRq : Set immediately after the last bit of the transmitted data was sent out
* - [5] RxIRq : receiver has detected the end of a valid data stream.
* - [4] IdleIRq : When the CommandReg changes its value from any command to the Idle command, or when an unknown command is 
*                 started will set the IdleIRq bit.
* - [3] HiAlertIRq : The Status1Reg register’s HiAlert bit is set.
* - [2] LoAlertIRq : Status1Reg register’s LoAlert bit is set.
* - [1] ErrIRq : any error bit in the ErrorReg register is set.
* - [0] TimerIRq : the timer decrements the timer value in register TCounterValReg to zero. */
#define RC522_SET_1_BIT         BIT(7U)
#define RC522_TX_IRQ_BIT        BIT(6U)
#define RC522_RX_IRQ_BIT        BIT(5U)
#define RC522_IDLE_IRQ_BIT      BIT(4U)
#define RC522_HALERT_IRQ_BIT    BIT(3U)
#define RC522_LALERT_IRQ_BIT    BIT(2U)
#define RC522_ERR_IRQ_BIT       BIT(1U)
#define RC522_TIMER_IRQ_BIT     BIT(0U)
#define RC522_GET_IRQ_BIT(regVal, bitMask)      (uint8_t)((uint8_t)(regVal) & (uint8_t)(bitMask))
#define RC522_CLEAR_ALL_IRQ_BIT                 (0x7F)

/* MFRC522 ComIEnReg : Control bits to enable and disable the passing of interrupt requests 
* - [7] IRqInv : Default assert level HIGH (Signal on pin IRQ is inverted with respect to the Status1Reg register’s IRq bit)
* - [6] TxIEn : allows the transmitter interrupt request (TxIRq bit) to be propagated to pin IRQ
* - [5] RxIEn : allows the receiver interrupt request (RxIRq bit) to be propagated to pin IRQ
* - [4] IdleIEn : allows the idle interrupt request (IdleIRq bit) to be propagated to pin IRQ
* - [3] HiAlertIEn : allows the high alert interrupt request (HiAlertIRq bit) to be propagated to pin IRQ
* - [2] LoAlertIEn : allows the low alert interrupt request (LoAlertIRq bit) to be propagated to pin IRQ
* - [1] ErrIEn : allows the error interrupt request (ErrIRq bit) to be propagated to pin IRQ
* - [0] TimerIEn : allows the timer interrupt request (TimerIRq bit) to be propagated to pin IRQ
*/
#define RC522_EN_TX_IRQ_MASK        BIT(6U)
#define RC522_EN_RX_IRQ_MASK        BIT(5U)
#define RC522_EN_IDLE_IRQ_MASK      BIT(4U)
#define RC522_EN_FIFO_HI_IRQ_MASK   BIT(3U)
#define RC522_EN_FIFO_LO_IRQ_MASK   BIT(2U)
#define RC522_EN_ERR_IRQ_MASK       BIT(1U)
#define RC522_EN_TMR_IRQ_MASK       BIT(0U)
#define RC522_EN_IRQ_CMD(bitMask)   (uint8_t)(BIT(7U) | bitMask)

/** Define driver error code **/
#define RC522_ETIMEOUT          (1U)    /* Error code for transmission timeout */
#define RC522_ECOM              (2U)    /* Error code for transmission error */
#define RC522_ENOROOM           (3U)    /* Error code for buffer Size not sufficient */
#define RC522_ENOSUPPORT        (4U)    /* Error code for software currently not supported */

/* Callback function after driver receive valid UID, user can use this function to further process the UID */
typedef void (*rc522_uid_cb_t)(const uint8_t *uid, const uint8_t uidSize);

/* Define MFRC522 public APIs struct, come with MFRC522 device instance after created */
struct rc522_api {
    /**
    * @brief Call this function and MFRC522 will start looking for RFID cards
    * @note This function is non-blocking using interrupt. Multithreading is implemented.
    * @param[I] dev      MFRC522 device structure provide SPI config and runtime data.
    * @param[I] cb       Reference to callback function using UID as user want.
    * @return Error code
    *   0 : Success capture UID
    *  -1 : Failed as an error has occurred
    */
    void (*startScanning)(const struct device* dev, const rc522_uid_cb_t cb);
    /**
     * @brief Call this function to get currently saved UID, it maybe not the latest UID that has been detected, so
     * I recommend user to use callback function when calling startScanning() API instead for the latest detected UID.
     * @param[I] dev        MFRC522 device structure provide SPI config and runtime data.
     * @param[I] rBuff      Receive buffer provided by user, store UID infomation.
     * @param[I] buffSize   Size of @param rBuff , at least 4 byte.
     */
    void (*getUID)       (const struct device* dev, uint8_t* rBuff,const uint8_t buffSize);
};

/* Define configuration structure */
struct rc522_config {
    uint32_t id;                    /* MFRC522 instance index */
    struct spi_dt_spec spi;         /* SPI device port for this instance */
    struct gpio_dt_spec irq_pin;    /* Pin configure to receive interrupt from RC522 and handle by callback ISR */
};

/* Define MFRC522 runtime data structure */
struct rc522_data {
    uint8_t uid[RC522_CL_1_BYTES_NUM];  /* Store UID of detected PICC */
    struct k_sem irq_sem;               /* Semaphore to unblocked IRQ handler thread */
    struct k_sem scan_sem;              /* Semaphore to unblocked start new scan thread */
    struct gpio_callback irq_gpio_cb;   /* GPIO callback struct, can be init with a GPIO callback function and assigned to a GPIO 
                                        device */
    rc522_uid_cb_t card_detected_cb;    /* Callback function when card is detected and UID is received */
};

#endif  /* ZEPHYR_DRIVERS_RC522_H_ */