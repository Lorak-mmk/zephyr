/*
 * Copyright (c) 2019 Brett Witherspoon
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/init.h>

#include <driverlib/setup.h>

static int ti_cc26xx_init(void)
{

	/* Performs necessary trim of the device. */
	SetupTrimDevice();

	return 0;
}

SYS_INIT(ti_cc26xx_init, PRE_KERNEL_1, 0);
