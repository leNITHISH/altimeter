#include <8052.h>
#include "i2c.h"
#include "delay.h"

#define SCL P1_0
#define SDA P1_1

/* writing 1 releases a line: the quasi-bidirectional port only has a weak
 * pull-up, so the slave can always pull it low */
#define I2C_DLY() delay_us(1)

void i2c_init(void)
{
	uint8_t i;

	SDA = 1;
	SCL = 1;
	I2C_DLY();
	/* a slave left mid-byte by a reset holds SDA low; clock it free */
	for (i = 0; i < 9 && !SDA; i++) {
		SCL = 0;
		I2C_DLY();
		SCL = 1;
		I2C_DLY();
	}
	i2c_start();
	i2c_stop();
}

void i2c_start(void)
{
	SDA = 1;
	SCL = 1;
	I2C_DLY();
	SDA = 0;
	I2C_DLY();
	SCL = 0;
	I2C_DLY();
}

void i2c_stop(void)
{
	SDA = 0;
	I2C_DLY();
	SCL = 1;
	I2C_DLY();
	SDA = 1;
	I2C_DLY();
}

uint8_t i2c_write(uint8_t b)
{
	uint8_t i, ack;

	for (i = 0; i < 8; i++) {
		SDA = (b & 0x80) ? 1 : 0;
		I2C_DLY();
		SCL = 1;
		I2C_DLY();
		SCL = 0;
		b <<= 1;
	}
	SDA = 1;			/* release for ACK */
	I2C_DLY();
	SCL = 1;
	I2C_DLY();
	ack = !SDA;
	SCL = 0;
	I2C_DLY();
	return ack;
}

uint8_t i2c_read(uint8_t ack)
{
	uint8_t i, v = 0;

	SDA = 1;			/* release so the slave can drive it */
	for (i = 0; i < 8; i++) {
		I2C_DLY();
		SCL = 1;
		I2C_DLY();
		v <<= 1;
		if (SDA)
			v |= 1;
		SCL = 0;
	}
	SDA = ack ? 0 : 1;
	I2C_DLY();
	SCL = 1;
	I2C_DLY();
	SCL = 0;
	SDA = 1;
	I2C_DLY();
	return v;
}

uint8_t i2c_probe(uint8_t addr)
{
	uint8_t ack;

	i2c_start();
	ack = i2c_write(addr << 1);
	i2c_stop();
	return ack;
}

uint8_t i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t val)
{
	uint8_t ok;

	i2c_start();
	ok = i2c_write(addr << 1) && i2c_write(reg) && i2c_write(val);
	i2c_stop();
	return ok;
}

uint8_t i2c_read_regs(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t n)
{
	i2c_start();
	if (!i2c_write(addr << 1) || !i2c_write(reg)) {
		i2c_stop();
		return 0;
	}
	i2c_start();			/* repeated start */
	if (!i2c_write((addr << 1) | 1)) {
		i2c_stop();
		return 0;
	}
	while (n--) {
		*buf++ = i2c_read(n != 0);
	}
	i2c_stop();
	return 1;
}
