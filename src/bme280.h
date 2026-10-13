#ifndef BME280_H
#define BME280_H

#include <stdint.h>

#define BME280_ID 0x60
#define BMP280_ID 0x58

/* probe 0x76/0x77, load calibration, start normal mode.
 * returns chip ID (0x60 / 0x58) or 0 if not found */
uint8_t bme280_init(void);

/* t: 0.01 degC, p: Pa. returns 1 on success */
uint8_t bme280_read(int32_t *t, uint32_t *p);

#endif
