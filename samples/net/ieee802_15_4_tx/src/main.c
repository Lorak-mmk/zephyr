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
LOG_MODULE_REGISTER(ieee802_15_4_tx, 10);

#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/random/random.h>

#include <zephyr/net/buf.h>
#include <net_private.h>
#include <zephyr/net/ieee802154_radio.h>

/* ieee802.15.4 device */
static struct ieee802154_radio_api *radio_api;
static const struct device *const ieee802154_dev =
	DEVICE_DT_GET(DT_CHOSEN(zephyr_ieee802154));

static void tx_thread()
{
	LOG_DBG("TX thread started");

	struct net_pkt *pkt;
	struct net_buf *buf;

	pkt = net_pkt_alloc_with_buffer(NULL, 256,
							AF_UNSPEC, 0,
							K_NO_WAIT);
	if (!pkt) {
		LOG_ERR("No more buffers");
		return;
	}

	buf = net_buf_frag_last(pkt->buffer);
	//*buf->data &= !IEEE802154_AR_FLAG_SET;
	net_buf_add_mem(buf, "abcdefgh12349876", 16);

	LOG_DBG("tx thread pkt %p buf %p", pkt, buf);
	LOG_HEXDUMP_DBG(buf->data, buf->len, "DATA >");

	while (true) {
		LOG_DBG("calling tx!");
		int ret = radio_api->tx(ieee802154_dev, IEEE802154_TX_MODE_CSMA_CA,
					pkt, buf);
		if (ret) {
			LOG_ERR("Error transmit data");
			break;
		} else {
			LOG_DBG("send packet!");
		}
		k_sleep(K_MSEC(1000));
		// net_pkt_unref(pkt);
	}
}


static bool init_ieee802154(void)
{
	LOG_INF("Initialize ieee802.15.4");

	if (!device_is_ready(ieee802154_dev)) {
		LOG_ERR("IEEE 802.15.4 device not ready");
		return false;
	}

	radio_api = (struct ieee802154_radio_api *)ieee802154_dev->api;


	LOG_INF("Set channel %u", 11);
	radio_api->set_channel(ieee802154_dev, 11);

	/* Start ieee802154 */
	radio_api->start(ieee802154_dev);

	return true;
}


int main(void)
{
	LOG_INF("Starting ieee802.15.4 TX example application");

	/* Initialize ieee802154 device */
	if (!init_ieee802154()) {
		LOG_ERR("Unable to initialize ieee802154");
		return 0;
	}

	/* Initialize RX thread */
	tx_thread();

	LOG_DBG("DONE MAIN");

	return 0;
}
