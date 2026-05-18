#ifndef ZEPHYR_DRIVERS_BH1750_H_
#define ZEPHYR_DRIVERS_BH1750_H_

#include <errno.h>
#include <zephyr/drivers/i2c.h>

/* BH1750 addresses 
 * 0x23 : BH1750 address(ADDR = 'L') */
#define BH1750_ADDR   0x23

/* BH1750 instruction set
 * 0x00 : Power down, BH1750 in no active state
 * 0x01 : Power on, BH1750 waiting for measurement command
 * 0x07 : Reset data register value. Reset command is not acceptable in Power down mode
 * 0x10 : Continuously H-resolution mode(measurement at 1 lx resolution)
 * 0x11 : Continuously H-resolution mode 2 (measurement at 0.5lx resolution)
 * 0x13 : Continuously L-resolution mode (measurement at 4 lx resolution)
 * 0x20 : One Time H-resolution mode, automatically set to Power down after measurement
 * 0x21 : One Time H-resolution mode 2, automatically set to Power down after measurement
 * 0x23 : One Time L-resolution mode, automatically set to Power down after measurement */
#define BH1750_PW_DOWN         0x00
#define BH1750_PW_ON           0x01
#define BH1750_RESET           0x07
#define BH1750_H_RES_CONT      0x10
#define BH1750_H_RES_CONT_2    0x11
#define BH1750_L_RES_CONT      0x13
#define BH1750_H_RES_ONESHOT   0x20
#define BH1750_H_RES_ONESHOT_2 0x21
#define BH1750_L_RES_ONESHOT   0x23

/* BH1750 measurement time constant
* 180ms : H-Resolution Modes 1st measurement time
* 120ms : H-Resolution Mode and H-Resolution Mode2 continuous measurement time
* 24ms : L-Resolution Mode 1st measurement time
* 16ms : L-Resolution Mode continuous measurement time */
#define BH1750_H_RES_START_T    180u
#define BH1750_H_RES_CONT_T     120u
#define BH1750_L_RES_START_T    24u
#define BH1750_L_RES_CONT_T     16u

/* Bh1750 measurement accuracy factor */
#define BH1750_FACTOR           1.2

/* Helper function, BH1750 light sensor after measurement will return 2 bytes(16-bit) result but
it will send 2 frame, each frame has 1 byte data that is High Byte [15:8] and Low Byte [7:0], so this
function dedicated to concatenate these bytes and return 1 result 16-bit, user can calculate with this
result later */
#define BH1750_CONCATE_RESULT(highByte, lowByte)    (uint16_t)(((uint16_t)(highByte) << 8u) | (uint16_t)(lowByte))

/* Declare BH1750 light sensor public APIs */
struct bh1750_api {
    int (*getData)(const struct device* dev, uint16_t* rBuff);   /* General get data API */
};

/* Define configuration structure */
struct bh1750_config {
    uint32_t id;                /* BH1750 device instance index */
    uint32_t mode;              /* Determined BH1750 operation mode through enum mode structure in binding .yaml file */
    struct i2c_dt_spec i2c;     /* i2c device port for BH1750 working in I2C protocol */
};

#endif /* ZEPHYR_DRIVERS_BH1750_H_ */