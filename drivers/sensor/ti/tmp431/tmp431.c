/*
 * Copyright (c) 2016 Firmwave
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ti_tmp431

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/logging/log.h>
#include "tmp431.h"

LOG_MODULE_REGISTER(TMP431, CONFIG_SENSOR_LOG_LEVEL);

// static int tmp431_reg_read8(const struct tmp431_config *cfg,
// 			   uint8_t reg, uint8_t *val)
// {
// 	if (i2c_burst_read_dt(&cfg->bus, reg, val, sizeof(*val)) < 0) {
// 		return -EIO;
// 	}

// 	return 0;
// }

static int tmp431_reg_write8(const struct tmp431_config *cfg,
			    uint8_t reg, uint8_t val)
{
	uint8_t buf[2];

	buf[0] = reg;
	buf[1] = val;

	return i2c_write_dt(&cfg->bus, buf, sizeof(buf));
}

// Used to read temperature
static int tmp431_reg_read_buf(const struct tmp431_config *cfg,
			   uint8_t reg, uint8_t *val, uint32_t len)
{
	if (i2c_burst_read_dt(&cfg->bus, reg, val, len) < 0) {
		return -EIO;
	}

	return 0;
}

static uint8_t set_config_flags(uint8_t old_config, uint8_t mask, uint8_t value)
{
	return (old_config & ~mask) | (value & mask);
}

static int tmp431_update_config_common(const struct tmp431_config *cfg, 
			uint8_t *reg_ptr, uint8_t reg_addr, uint8_t mask, uint8_t val)
{
	int rc;
	const uint8_t new_val = set_config_flags(*reg_ptr, mask, val);

	rc = tmp431_reg_write8(cfg, reg_addr, new_val);
	if (rc == 0) {
		*reg_ptr = new_val;
	}

	return rc;
}

static int tmp431_update_config(const struct device *dev, uint8_t mask,
				uint8_t val)
{
	struct tmp431_data *data = dev->data;
	return tmp431_update_config_common(dev->config, &data->config_reg, TMP431_REG_ADDR_CONF1_WRITE, mask, val);
}

static int tmp431_update_config2(const struct device *dev, uint8_t mask,
				uint8_t val)
{
	struct tmp431_data *data = dev->data;
	return tmp431_update_config_common(dev->config, &data->config2_reg, TMP431_REG_ADDR_CONF2_WRITE, mask, val);
}

static int tmp431_update_convrate(const struct device *dev, uint8_t value)
{
	struct tmp431_data *data = dev->data;
	__ASSERT_NO_MSG((value & TMP431_CONV_RATE_MASK) == 0);
	data->conv_rate_reg = value;
	return tmp431_update_config_common(dev->config, &data->conv_rate_reg, TMP431_REG_ADDR_CONV_RATE_WRITE, 0, 0);
}

static int tmp431_attr_set(const struct device *dev,
			   enum sensor_channel chan,
			   enum sensor_attribute attr,
			   const struct sensor_value *val)
{
	uint8_t value;
	uint32_t cr;

	if (chan != SENSOR_CHAN_AMBIENT_TEMP) {
		return -ENOTSUP;
	}

	switch (attr) {
#if CONFIG_TMP431_FULL_SCALE_RUNTIME
	case SENSOR_ATTR_FULL_SCALE:
		/* the sensor supports two ranges 0 to 127 and -64 to 191 */
		/* the value contains the upper limit */
		if (val->val1 == 127) {
			value = 0x0000;
		} else if (val->val1 == 191) {
			value = TMP431_CONFIG_EM;
		} else {
			return -ENOTSUP;
		}

		if (tmp431_update_config(dev, TMP431_CONFIG_EM, value) < 0) {
			LOG_DBG("Failed to set attribute!");
			return -EIO;
		}
		break;
#endif
	case SENSOR_ATTR_SAMPLING_FREQUENCY:
#if CONFIG_TMP431_SAMPLING_FREQUENCY_RUNTIME
		/* conversion rate in tenths of mHz */
		cr = val->val1 * 10000 + val->val2 / 100;

		/* the sensor supports 0.0625Hz, 0.125Hz, 0.25Hz, 0.5Hz, 1Hz, 2Hz, 4Hz and 8Hz */
		/* conversion rate */
		switch (cr) {
		case 625:
			value = TMP431_CONV_RATE_00625;
			break;
		case 1250:
			value = TMP431_CONV_RATE_0125;
			break;
		case 2500:
			value = TMP431_CONV_RATE_025;
			break;
		case 5000:
			value = TMP431_CONV_RATE_05;
			break;
		case 10000:
			value = TMP431_CONV_RATE_1;
			break;
		case 20000:
			value = TMP431_CONV_RATE_2;
			break;
		case 40000:
			value = TMP431_CONV_RATE_4;
			break;
		case 80000:
			value = TMP431_CONV_RATE_8;
			break;

		default:
			return -ENOTSUP;
		}

		if (tmp431_update_config(dev, TMP431_CONV_RATE_MASK, value) < 0) {
			LOG_DBG("Failed to set attribute!");
			return -EIO;
		}

		break;
#endif

	default:
		return -ENOTSUP;
	}

	return 0;
}

static int tmp431_sample_fetch(const struct device *dev,
			       enum sensor_channel chan)
{
	struct tmp431_data *drv_data = dev->data;
	const struct tmp431_config *cfg = dev->config;
	uint8_t sample[2];

	__ASSERT_NO_MSG(chan == SENSOR_CHAN_ALL || chan == SENSOR_CHAN_AMBIENT_TEMP);

	if (tmp431_reg_read_buf(cfg, TMP431_REG_ADDR_LOCAL_TEMP, sample, 2) < 0) {
		return -EIO;
	}

	drv_data->sample[0] = sample[0];
	drv_data->sample[1] = sample[1];

	return 0;
}

static int tmp431_channel_get(const struct device *dev,
			      enum sensor_channel chan,
			      struct sensor_value *val)
{
	struct tmp431_data *drv_data = dev->data;
	const struct tmp431_config *cfg = dev->config;

	if (chan != SENSOR_CHAN_AMBIENT_TEMP) {
		return -ENOTSUP;
	}

	int32_t sample_integer = (int32_t)drv_data->sample[0];
	int32_t sample_fractional = (int32_t)(drv_data->sample[1] >> 4);
	val->val1 = cfg->extended_mode ? (sample_integer - 64) : (sample_integer);
	// Zephyr expects fractional part to be in 1/10^6 resolution.
	// The resolution from theremometer is 0.0625
	// 0.0625 / 10^(-6) = 62500
	val->val2 = sample_fractional * 62500;

	return 0;
}

static const struct sensor_driver_api tmp431_driver_api = {
	.attr_set = tmp431_attr_set,
	.sample_fetch = tmp431_sample_fetch,
	.channel_get = tmp431_channel_get,
};

int tmp431_init(const struct device *dev)
{
	const struct tmp431_config *cfg = dev->config;
	struct tmp431_data *data = dev->data;

	if (cfg->gpio_power.port) {
		if (!gpio_is_ready_dt(&cfg->gpio_power)) {
			LOG_ERR("Cannot get pointer to gpio supply device");
			return -ENODEV;
		}

		int rc = gpio_pin_configure_dt(&cfg->gpio_power, GPIO_OUTPUT_HIGH);
		if (rc) {
			LOG_ERR("Failed to configure supply pin");
			return rc;
		}

		k_msleep(20);
	}

	if (!device_is_ready(cfg->bus.bus)) {
		LOG_ERR("I2C dev %s not ready", cfg->bus.bus->name);
		return -EINVAL;
	}

	data->config_reg = (cfg->extended_mode ? TMP431_CONFIG_EM : 0)
					| TMP431_CONFIG_MASK /* for now this driver doesn't use ALERT / THERM */;
	int ret = tmp431_update_config(dev, 0, 0);
	if (ret < 0) {
		return ret;
	}

	// Overwriting to disable remote channel which we don't use
	data->config2_reg = TMP431_CONFIG2_LOCAL | TMP431_CONFIG2_RC;
	ret = tmp431_update_config2(dev, 0, 0);
	if (ret < 0) {
		return ret;
	}

	ret = tmp431_update_convrate(dev, cfg->cr);

	return ret;
}


#define TMP431_INST(inst)						    \
	static struct tmp431_data tmp431_data_##inst;			    \
	static const struct tmp431_config tmp431_config_##inst = {	    \
		.bus = I2C_DT_SPEC_INST_GET(inst),			    \
		.cr = DT_INST_ENUM_IDX(inst, conversion_rate),		     \
		.extended_mode = DT_INST_PROP(inst, extended_mode),	    \
		.gpio_power = GPIO_DT_SPEC_INST_GET(inst, supply_gpios) \
	};								    \
									    \
	SENSOR_DEVICE_DT_INST_DEFINE(inst, tmp431_init, NULL, &tmp431_data_##inst, \
			      &tmp431_config_##inst, POST_KERNEL,	    \
			      CONFIG_SENSOR_INIT_PRIORITY, &tmp431_driver_api);

DT_INST_FOREACH_STATUS_OKAY(TMP431_INST)
