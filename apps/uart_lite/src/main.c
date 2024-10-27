/*
 * Copyright (c) 2022 Libre Solar Technologies GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#include <string.h>

/* change this to any other UART peripheral if desired */
#define UART_DEVICE_NODE DT_ALIAS(uart_lite)

#define SLEEP_MS 1

static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);


void print_uart(char *buf, size_t len)
{
	uart_tx(uart_dev, buf, len, SYS_FOREVER_US);
}

int main(void)
{
	if (!device_is_ready(uart_dev)) {
		printk("UART device not found!");
		return 0;
	}

	char buffer[100];

	int i = 0;
	while (true) {
		int len = sprintf(buffer, "UART LITE MESSAGE %d\r\n", i);
		//printk("printk message\r\n");
		print_uart(buffer, len);
		k_msleep(SLEEP_MS);
		i += 1;
	}
	return 0;
}
