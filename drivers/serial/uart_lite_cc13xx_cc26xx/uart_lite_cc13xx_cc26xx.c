/*
 * Copyright (c) 2019 Brett Witherspoon
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ti_cc13xx_cc26xx_uart_lite

#include <zephyr/device.h>
#include <errno.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/policy.h>
#include <zephyr/drivers/uart.h>
#include <driverlib/prcm.h>
#include <driverlib/uart.h>

#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26X2.h>
#include <zephyr/irq.h>
#include <driverlib/aon_wuc.h>
#include "scif_uart_emulator.h"
#include <driverlib/aux_wuc.h>

struct uart_cc13xx_cc26xx_config {
};

struct uart_cc13xx_cc26xx_data {
	uint32_t lost_bytes;
#ifdef CONFIG_UART_ASYNC_API
	uart_callback_t callback;
	void *user_data;
#endif
};

static void uart_lite_cc13xx_cc26xx_poll_out(const struct device *dev,
					unsigned char c)
{
	//printk("POLL OUT. Count: %d\r\n", scifUartGetTxFifoCount());
	while ((SCIF_UART_TX_FIFO_MAX_COUNT - scifUartGetTxFifoCount()) == 0) { }
	scifUartTxPutChar(c);
}

#ifdef CONFIG_UART_ASYNC_API
static int uart_lite_cc13xx_cc26xx_callback_set(const struct device *dev,
			    uart_callback_t callback,
			    void *user_data)
{
	struct uart_cc13xx_cc26xx_data *data = dev->data;
	data->callback = callback;
	data->user_data = user_data;

	return 0;
}

static size_t cells(size_t characters) {
	return (characters >> 1) + (characters & 1);
}

static int uart_lite_cc13xx_cc26xx_tx(const struct device *dev, const uint8_t *buf, size_t len,
		  int32_t timeout)
{
	struct uart_cc13xx_cc26xx_data *data = dev->data;

	__ASSERT(len <= (SCIF_UART_TX_FIFO_MAX_COUNT * 2), "Too long message for UART Lite: %zu", len);

	uint32_t free_cells = (SCIF_UART_TX_FIFO_MAX_COUNT - scifUartGetTxFifoCount());

	if (data->lost_bytes > 0) {
		// We already lost some bytes.
		char lost_buffer[27];
		int written = sprintf(lost_buffer, "\r\nLOST BYTES: %" PRIu32 "\r\n", data->lost_bytes);
		if (free_cells < (cells(written) + cells(len))) {
			// If we can't print both the LOST message and the new buffer
			// then don't print anything. Otherwise we get a lot of
			// messages cut to first few characters. It is better
			// to print full messages and skip some of them.
			data->lost_bytes += len;
			goto end;
		}
		scifUartTxPutChars(lost_buffer, written);
		free_cells -= cells(written);
		data->lost_bytes = 0;
	}

	if (cells(len) > free_cells) {
		// Not enough space to write everything.
		data->lost_bytes = (len - (free_cells * 2));
		len = free_cells * 2;
	}

	scifUartTxPutChars(buf, len);

end:

	if (data->callback) {
		struct uart_event event = {
			.type = UART_TX_DONE,
			.data.tx = {
				.buf = buf,
				.len = len,
			},
		};
		data->callback(dev, &event, data->user_data);
	}

	return 0;
}
#endif

#ifdef CONFIG_UART_USE_RUNTIME_CONFIGURE
static int uart_lite_cc13xx_cc26xx_configure(const struct device *dev,
					const struct uart_config *cfg)
{
	return 0;
}

static int uart_lite_cc13xx_cc26xx_config_get(const struct device *dev,
					 struct uart_config *cfg)
{
	return 0;
}
#endif /* CONFIG_UART_USE_RUNTIME_CONFIGURE */


static const struct uart_driver_api uart_lite_cc13xx_cc26xx_driver_api = {
	.poll_out = uart_lite_cc13xx_cc26xx_poll_out,
#ifdef CONFIG_UART_ASYNC_API
	.callback_set = uart_lite_cc13xx_cc26xx_callback_set,
	.tx = uart_lite_cc13xx_cc26xx_tx,
#endif
#ifdef CONFIG_UART_USE_RUNTIME_CONFIGURE
	.configure = uart_lite_cc13xx_cc26xx_configure,
	.config_get = uart_lite_cc13xx_cc26xx_config_get,
#endif
};

static int uart_lite_cc13xx_cc26xx_init(const struct device *dev)
{
	printk("UART LITE INIT\r\n");
    AONWUCAuxWakeupEvent(AONWUC_AUX_WAKEUP);
    while(!(AONWUCPowerStatusGet() & AONWUC_AUX_POWER_ON)) {};

	// aux_ctrl_register_consumer
	bool interrupts_disabled = IntMasterDisable();
    AONWUCAuxWakeupEvent(AONWUC_AUX_WAKEUP);
  	while(!(AONWUCPowerStatusGet() & AONWUC_AUX_POWER_ON)) {};
	AUXWUCClockEnable(AUX_WUC_SMPH_CLOCK);
  	while(AUXWUCClockStatus(AUX_WUC_SMPH_CLOCK) != AUX_WUC_CLOCK_READY);
	if(!interrupts_disabled) {
    	IntMasterEnable();
  	}

    AONWUCMcuPowerDownConfig(AONWUC_CLOCK_SRC_LF);
    AONWUCAuxPowerDownConfig(AONWUC_CLOCK_SRC_LF);

    int result = scifInit(&scifDriverSetup);
	printk("scifInit result: %d\r\n", result);
    scifResetTaskStructs((1 << SCIF_UART_EMULATOR_TASK_ID), (1 << SCIF_STRUCT_CFG) | (1 << SCIF_STRUCT_INPUT) | (1 << SCIF_STRUCT_OUTPUT));
    result = scifExecuteTasksOnceNbl(1 << SCIF_UART_EMULATOR_TASK_ID);
	printk("scifInit scifExecuteTasksOnceNbl: %d\r\n", result);

    scifUartSetBaudRate(SCIF_UART_BAUD_RATE);

	return 0;
}
	
static const struct uart_cc13xx_cc26xx_config
	uart_lite_cc13xx_cc26xx_config = {
};

static struct uart_cc13xx_cc26xx_data
	uart_lite_cc13xx_cc26xx_data = {
		.lost_bytes = 0,
#ifdef CONFIG_UART_ASYNC_API
		.callback = NULL,
		.user_data = NULL,
#endif
		.printed_warning = false,
};

DEVICE_DT_INST_DEFINE(0,					     \
	uart_lite_cc13xx_cc26xx_init,				     \
	PM_DEVICE_DT_INST_GET(n),				     \
	&uart_lite_cc13xx_cc26xx_data, &uart_lite_cc13xx_cc26xx_config,\
	PRE_KERNEL_1, CONFIG_SERIAL_INIT_PRIORITY,		     \
	&uart_lite_cc13xx_cc26xx_driver_api)



