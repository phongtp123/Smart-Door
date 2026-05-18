#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdint.h>
#include "rc522.h"

/* Get RC522 device node structure */
static const struct device* rc522 = DEVICE_DT_GET(DT_ALIAS(my_rc522));
/* Register logging module on main application for debug purpose */
LOG_MODULE_REGISTER(smart_door);

/* Callback function pass to MFRC522 driver */
void handle_MFRC522_UID(const uint8_t *uid, const uint8_t uidSize)
{
    /* Iterate variable */
    uint8_t i = 0u;

    /* Simple callback now, print UID to console when UID is detected */
    printk("UID: ");
    for(i = 0u; i < uidSize; i++)
    {
        printk("%x ", uid[i]);
    }
    printk("\r\n");
}

int main(void)
{
    /* Return value, expected 0 to process OK */
    int ret = 0u;
    
    /* Check MFRC522 should be ready before using */
    if(!device_is_ready(rc522))
    {
        LOG_ERR("[ERROR (%d)]: MFRC522 device is not ready to use\r\n", ret);
        ret = -ENODEV;
    }
    else
    {
        LOG_INF("MFRC522 is ready\r\n");
    }
    /* Application start only when all device is ready */
    if(!ret)
    {
        /* Get RC522 device driver API */
        const struct rc522_api* rc522_api_funcs = (const struct rc522_api*)rc522->api;
        rc522_api_funcs->startScanning(rc522, handle_MFRC522_UID);
    }

    while(1)
    {
        k_msleep(1000);
    }

    return ret;
}