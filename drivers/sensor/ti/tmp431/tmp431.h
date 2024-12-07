/*
 * Copyright (c) 2020 Innoseis BV
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_TMP431_TMP431_H_
#define ZEPHYR_DRIVERS_SENSOR_TMP431_TMP431_H_

#include <zephyr/device.h>
#include <zephyr/sys/util.h>

#define TMP431_REG_ADDR_LOCAL_TEMP      0x00
#define TMP431_REG_ADDR_CONF1_WRITE     0x09
#define TMP431_REG_ADDR_CONF2_WRITE     0x1A
#define TMP431_REG_ADDR_CONV_RATE_WRITE     0x0A

#define TMP431_CONFIG_EM    BIT(2)
#define TMP431_CONFIG_AL_TH	BIT(5)
#define TMP431_CONFIG_SD	BIT(6)
#define TMP431_CONFIG_MASK	BIT(7)

#define TMP431_CONFIG2_RC	BIT(2)
#define TMP431_CONFIG2_LOCAL	BIT(3)
#define TMP431_CONFIG2_REMOTE	BIT(4)
#define TMP431_CONFIG2_REMOTE2	BIT(5)

#define TMP431_CONV_RATE_MASK (BIT(0) | BIT(1) | BIT(2) | BIT(3))
#define TMP431_CONV_RATE_00625  0
#define TMP431_CONV_RATE_0125   1
#define TMP431_CONV_RATE_025    2
#define TMP431_CONV_RATE_05     3
#define TMP431_CONV_RATE_1	    4
#define TMP431_CONV_RATE_2	    5
#define TMP431_CONV_RATE_4      6
#define TMP431_CONV_RATE_8      7

struct tmp431_data {
	uint8_t sample[2];
	uint8_t config_reg;
	uint8_t config2_reg;
	uint8_t conv_rate_reg;
};

struct tmp431_config {
	const struct i2c_dt_spec bus;
	struct gpio_dt_spec gpio_power;
	uint8_t cr;
	bool extended_mode : 1;
};

#endif
