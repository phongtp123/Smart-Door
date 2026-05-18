/* Binding file "custom,bh1750.yaml" implementation located */
#define DT_DRV_COMPAT custom_bh1750

#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include "bh1750.h"

/* Register BH1750 custom module logging */
LOG_MODULE_REGISTER(bh1750);

/***************************************************************************************************************
* PROTOTYPE DECLARATIONS
****************************************************************************************************************/
/**
* @brief Intitialize BH1750 device ready for transmit data back to master device. This include send command
* start BH1750, command to operate in H-resolution or L-resolution and working in one-shot or continuous mode,
* these operation command change according to mode and continuous boolean parameter in device config structure.
* @note This function is private and cannot be used by user. When user define a device tree node that compatible
* with "custom,bh1750" then this function is run when MCU is booted and instance id of that node is assigned to 
* device config.
*
* @param dev    Reference to a node device structure, consist of config structure useful for init function and
*               api structure that include BH1750 public APIs
*/
static int bh1750_init(const struct device* dev);
/**
* @brief Function to retrive data from BH1750 sensor.
* @param dev   Reference to a node device structure, consist of config structure useful for init function and
*              api structure that include BH1750 public APIs.
* @param rBuff Reference to 16-bit buffer to receive data that retrived successful from BH1750
*/
static int bh1750_receive_data(const struct device* dev, uint16_t* rBuff);

/***************************************************************************************************************
* IMPLEMENTATION
****************************************************************************************************************/
static int bh1750_init(const struct device* dev)
{
    /* Return value equal to 0 mean process OK */
    int ret = 0u;
    /* Cast config structure in device structure into bh1750 config structure */
    const struct bh1750_config* conf = (const struct bh1750_config*)dev->config;
    /* Get i2c device port struct from device config */
    const struct i2c_dt_spec* i2c = &(conf->i2c);
    /* Get BH1750 operation mode from device config */
    const uint32_t mode = conf->mode;
    /* Start command buffer */
    uint8_t start[1] = {BH1750_PW_ON};
    /* Reset command buffer */
    uint8_t reset[1] = {BH1750_RESET};
    /* Operation mode command buffer */
    uint8_t opmode[1] = {};
    /* Operation command buffer (B1750 operation mode define in custom,bh1750.yaml binding file) */
    switch(mode)
    {
        /* H-resolution mode */
        case 0u:
        {
#ifdef CONFIG_BH1750_CONTINUOUS_MODE
            opmode[0] = BH1750_H_RES_CONT;
#else
            opmode[0] = BH1750_H_RES_ONESHOT;
#endif  /* CONFIG_BH1750_CONTINUOUS_MODE */
            break;
        }
        /* H-resolution mode 2 */
        case 1u:
        {
#ifdef CONFIG_BH1750_CONTINUOUS_MODE
            opmode[0] = BH1750_H_RES_CONT_2;
#else
            opmode[0] = BH1750_H_RES_ONESHOT_2;
#endif
            break;
        }
        /* L-resolution mode */
        case 2u:
        {
#ifdef CONFIG_BH1750_CONTINUOUS_MODE
            opmode[0] = BH1750_L_RES_CONT;
#else
            opmode[0] = BH1750_L_RES_ONESHOT;
#endif
            break;
        }
    }

    /* Start initializing BH1750 */
    LOG_DBG("Initializing BH1750 device (ID: %u)\r\n", conf->id);
    /* Check i2c device is ready (true is ready, false is error) */
    if(!i2c_is_ready_dt(i2c))
    {
        LOG_ERR("I2C Device is not ready\r\n");
        ret = -ENODEV;
    }
    else
    {
        /* Send start BH1750 command */
        ret = i2c_write_dt(i2c, start, sizeof(start));
        if (ret) 
        {
            LOG_ERR("Could not start BH1750\r\n");
        }
        else
        {
            /* Wait for 10ms for BH1750 to start properly */
            k_msleep(10);
            /* Send reset command to fresh start BH1750 */
            ret = i2c_write_dt(i2c, reset, sizeof(reset));
            if(ret)
            {
                LOG_ERR("Could not reset BH1750\r\n");
            }
            else
            {
                ret = i2c_write_dt(i2c, opmode, sizeof(opmode));
                if(ret)
                {
                    LOG_ERR("Could not send command to start measurement on BH1750\r\n");
                }
                else
                {
                    /* According to BH1750 datasheet , first measurement of H-resolution mode take typical 180ms to finish,
                     L-resolution mode measurement take about 24ms */
                    switch(mode)
                    {
                        case 0u:
                        case 1u:
                        {
                            k_msleep(BH1750_H_RES_START_T);
                            break;
                        }
                        case 2u:
                        {
                            k_msleep(BH1750_L_RES_START_T);
                            break;
                        }
                    }
                }
            }
        }
    }

    return ret;
}

static int bh1750_receive_data(const struct device* dev, uint16_t* rBuff)
{
    /* Return value equal to 0 mean process OK */
    int ret = 0u;
    /* Cast config structure in device structure into bh1750 config structure */
    const struct bh1750_config* conf = (const struct bh1750_config*)dev->config;
    /* Get i2c device port struct from device config */
    const struct i2c_dt_spec* i2c = &(conf->i2c);
    /* Get BH1750 operation mode from device config */
    const uint32_t mode = conf->mode;
    /* Data buffer */
    uint8_t data[2] = {};
    /* In case user want use BH1750 in One-Shot mode then need to send start device and start oneshot opmode manually
     each time user call API to receive data, in other hand Continuous mode no need to start all again manually. */
#ifndef CONFIG_BH1750_CONTINUOUS_MODE
    /* Start command buffer */
    uint8_t start[1] = {BH1750_PW_ON};
    uint8_t opmode[1] = {};
    /* Operation mode command buffer (B1750 operation mode define in custom,bh1750.yaml binding file) */
    switch(mode)
    {
        /* H-resolution mode (One-Shot) */
        case 0u:
        {
            opmode[0] = BH1750_H_RES_ONESHOT;
            break;
        }
        /* H-resolution mode 2 (One-Shot) */
        case 1u:
        {
            opmode[0] = BH1750_H_RES_ONESHOT_2;
            break;
        }
        /* L-resolution mode (One-Shot) */
        case 2u:
        {
            opmode[0] = BH1750_L_RES_ONESHOT;
            break;
        }
    }
    /* Send start BH1750 command */
    ret = i2c_write_dt(i2c, start, sizeof(start));
    if (ret) 
    {
        LOG_ERR("Could not start BH1750\r\n");
    }
    else
    {
        /* Send start one-shot operation command */
        ret = i2c_write_dt(i2c, opmode, sizeof(opmode));
        if (ret) 
        {
            LOG_ERR("Could not send command to start one-shot measurement on BH1750\r\n");
        }
    }

#endif  /* CONFIG_BH1750_CONTINUOUS_MODE */

    /* Wait for amount of measurement time. As BH1750 datasheet describe, after first measurement, normal measurement
     of H-resolution measurement take 120ms and L-measurement take 16ms to finish measure */
    switch(mode)
    {
        case 0u:
        case 1u:
        {
            k_msleep(BH1750_H_RES_CONT_T);
            break;
        }
        case 2u:
        {
            k_msleep(BH1750_L_RES_CONT_T);
            break;
        }
    }
    /* Read data from BH1750 */
    ret = i2c_read_dt(i2c, data, sizeof(data));
    if(ret)
    {
        LOG_ERR("Could not read measurement on BH1750\r\n");
    }
    else
    {
        /* Concatenate data, adjust result to reach accuracy and return final result to receive buffer */
        *rBuff = (uint16_t)(BH1750_CONCATE_RESULT(data[0], data[1]) / BH1750_FACTOR);
    }

    return ret;
}

/***************************************************************************************************************
* DEVICE TREE HANDLING
****************************************************************************************************************/
/* Construct BH1750 API structure (assign struct with static keyword as not public this struct for other module
except for this module, these API public with device node of "custom,bh1750" compatible) */
static const struct bh1750_api bh1750_api_funcs = {
    .getData = bh1750_receive_data,
};

/* Automate construct bh1750 config when a bh1750 device instance is created */
#define BH1750_CONFIG_INST(inst)                                     \
    static const struct bh1750_config bh1750_config_##inst = {       \
        .i2c = I2C_DT_SPEC_INST_GET(inst),                           \
        .mode = DT_INST_PROP(inst, mode),                            \
        .id = inst,                                                  \
    };

/* Initialize device driver for bh1750 when a bh1750 device instance is created */
#define BH1750_DEVICE_INST(inst)                                     \
    DEVICE_DT_INST_DEFINE(inst,                                      \
                          bh1750_init,                               \
                          NULL,                                      \
                          NULL,                                      \
                          &bh1750_config_##inst,                     \
                          POST_KERNEL,                               \
                          CONFIG_I2C_INIT_PRIORITY,                  \
                          &bh1750_api_funcs);

/* Function macro that construct config and initialize device driver for BH1750 at booting time  */
#define BH1750_INST(inst)       \
    BH1750_CONFIG_INST(inst)    \
    BH1750_DEVICE_INST(inst)
/* Each bh1750 instance is created in device tree will call BH1750_INST with different instance number */
DT_INST_FOREACH_STATUS_OKAY(BH1750_INST)
