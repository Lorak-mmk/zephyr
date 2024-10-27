/*
 * Copyright (c) 2016-2019 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief App implementing 802.15.4 "serial-radio" protocol
 *
 * Application implementing 802.15.4 "serial-radio" protocol compatible
 * with popular Contiki-based native border routers.
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ieee802_15_4_tx, 5);

#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/random/random.h>

#include <zephyr/net_buf.h>
#include <zephyr/net/ieee802154_radio.h>


int main(void)
{
	LOG_INF("Starting ieee802.15.4 TX example application");

	struct net_if* l2 = net_if_get_ieee802154();



	LOG_DBG("DONE MAIN");

	return 0;
}
