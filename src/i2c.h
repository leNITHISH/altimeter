#ifndef I2C_H
#define I2C_H

#include <stdint.h>

/* single-master bit-bang I2C on P1.0 (SCL) / P1.1 (SDA), ~50 kHz */

void i2c_init(void);			/* release bus, clock out any stuck slave */
void i2c_start(void);
void i2c_stop(void);
uint8_t i2c_write(uint8_t b);		/* returns 1 if slave ACKed */
uint8_t i2c_read(uint8_t ack);		/* ack = 1 for more bytes, 0 for last */

/* 7-bit address helpers; return 1 on success */
uint8_t i2c_probe(uint8_t addr);
uint8_t i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t val);
uint8_t i2c_read_regs(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t n);

#endif
