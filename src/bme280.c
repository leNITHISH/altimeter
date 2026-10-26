#include <stdint.h>
#include "bme280.h"
#include "i2c.h"
#include "delay.h"

#define REG_CALIB	0x88
#define REG_ID		0xD0
#define REG_RESET	0xE0
#define REG_CTRL_MEAS	0xF4
#define REG_CONFIG	0xF5
#define REG_DATA	0xF7

#define CONFIG_VAL	0x2C	/* standby 62.5 ms, IIR x8 (the Kalman filter does the rest) */
#define CTRL_MEAS_VAL	0x57	/* osrs_t x2, osrs_p x16, normal mode */

static uint8_t addr;

/* temperature and pressure trimming values (identical on BMP280).
 * registers 0x88..0x9F are little-endian in this exact order, and so is
 * sdcc's mcs51 layout, so the burst read lands straight in the struct.
 * kept in idata: the 128 bytes of direct RAM are too tight. */
static __idata struct {
	uint16_t T1;
	int16_t T2, T3;
	uint16_t P1;
	int16_t P2, P3, P4, P5, P6, P7, P8, P9;
} cal;

#define dig_T1 cal.T1
#define dig_T2 cal.T2
#define dig_T3 cal.T3
#define dig_P1 cal.P1
#define dig_P2 cal.P2
#define dig_P3 cal.P3
#define dig_P4 cal.P4
#define dig_P5 cal.P5
#define dig_P6 cal.P6
#define dig_P7 cal.P7
#define dig_P8 cal.P8
#define dig_P9 cal.P9

static __idata int32_t t_fine;

uint8_t bme280_init(void)
{
	uint8_t id;

	addr = 0x76;
	if (!i2c_read_regs(addr, REG_ID, &id, 1)) {
		addr = 0x77;
		if (!i2c_read_regs(addr, REG_ID, &id, 1))
			return 0;
	}
	if (id != BME280_ID && id != BMP280_ID)
		return 0;

	i2c_write_reg(addr, REG_RESET, 0xB6);
	delay_ms(5);

	if (!i2c_read_regs(addr, REG_CALIB, (uint8_t *)&cal, sizeof(cal)))
		return 0;

	/* config is only reliably written in sleep mode, so before ctrl_meas */
	i2c_write_reg(addr, REG_CONFIG, CONFIG_VAL);
	i2c_write_reg(addr, REG_CTRL_MEAS, CTRL_MEAS_VAL);
	return id;
}

/* Bosch datasheet 8.2, 32-bit integer versions. int is 16 bits on sdcc,
 * so every term is cast to int32_t explicitly. __reentrant puts the
 * 32-bit locals on the stack instead of the scarce direct RAM. */
static int32_t compensate_T(int32_t adc_T) __reentrant
{
	int32_t var1, var2, d;

	var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * (int32_t)dig_T2) >> 11;
	d = (adc_T >> 4) - (int32_t)dig_T1;
	var2 = (((d * d) >> 12) * (int32_t)dig_T3) >> 14;
	t_fine = var1 + var2;
	return (t_fine * 5 + 128) >> 8;
}

static uint32_t compensate_P(int32_t adc_P) __reentrant
{
	int32_t var1, var2;
	uint32_t p;

	var1 = (t_fine >> 1) - 64000L;
	var2 = (((var1 >> 2) * (var1 >> 2)) >> 11) * (int32_t)dig_P6;
	var2 = var2 + ((var1 * (int32_t)dig_P5) << 1);
	var2 = (var2 >> 2) + ((int32_t)dig_P4 << 16);
	var1 = ((((int32_t)dig_P3 * (((var1 >> 2) * (var1 >> 2)) >> 13)) >> 3) +
		(((int32_t)dig_P2 * var1) >> 1)) >> 18;
	var1 = ((32768L + var1) * (int32_t)dig_P1) >> 15;
	if (var1 == 0)
		return 0;	/* avoid division by zero */
	p = ((uint32_t)(1048576L - adc_P) - (uint32_t)(var2 >> 12)) * 3125UL;
	if (p < 0x80000000UL)
		p = (p << 1) / (uint32_t)var1;
	else
		p = (p / (uint32_t)var1) * 2;
	var1 = ((int32_t)dig_P9 * (int32_t)(((p >> 3) * (p >> 3)) >> 13)) >> 12;
	var2 = ((int32_t)(p >> 2) * (int32_t)dig_P8) >> 13;
	return (uint32_t)((int32_t)p + ((var1 + var2 + (int32_t)dig_P7) >> 4));
}

uint8_t bme280_read(int32_t *t, uint32_t *p)
{
	__idata uint8_t d[6];
	int32_t adc_P, adc_T;

	if (!i2c_read_regs(addr, REG_DATA, d, 6))
		return 0;
	adc_P = ((int32_t)d[0] << 12) | ((int32_t)d[1] << 4) | (d[2] >> 4);
	adc_T = ((int32_t)d[3] << 12) | ((int32_t)d[4] << 4) | (d[5] >> 4);
	if (adc_P == 0x80000L || adc_T == 0x80000L)
		return 0;	/* measurement skipped / not ready yet */

	*t = compensate_T(adc_T);
	*p = compensate_P(adc_P);
	return 1;
}
