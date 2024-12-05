#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(1KT_interop_demo, LOG_LEVEL_DBG);

#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include <zephyr/net_buf.h>
#include <zephyr/net/ieee802154_radio.h>

#include <zephyr/shell/shell.h>

#define IEEE802_15_4_CHANNEL 11
#define TEMP_TRANSFER_INTERVAL_SECS 5
#define SUFFIX_SIZE 2

struct TemperatureMeasurementMsg {
    uint32_t cherry_mote_id;
    uint32_t sequential_no;
    uint32_t timestamp;
    int32_t temperature_centigrades_celsius;
};


/* ieee802.15.4 device */
static struct ieee802154_radio_api *radio_api;
static const struct device *const ieee802154_dev =
	DEVICE_DT_GET(DT_CHOSEN(zephyr_ieee802154));

static const struct device *const thermometer = 
	DEVICE_DT_GET(DT_ALIAS(ambient_temp0));

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

static uint32_t global_sequence_number;
static struct TemperatureMeasurementMsg last_received_temperature = {
	.temperature_centigrades_celsius = 0xffffffff, // Special value to mark that no value was received yet
};

static uint32_t get_id() {
	return 0xdeadbeef;
}

static int32_t get_temperature() {
	struct sensor_value value;

	int ret = sensor_sample_fetch_chan(thermometer, SENSOR_CHAN_AMBIENT_TEMP);
	if (ret < 0) {
		LOG_ERR("Could not fetch temperature: %d\n", ret);
		return ret;
	}

	ret = sensor_channel_get(thermometer, SENSOR_CHAN_AMBIENT_TEMP, &value);
	if (ret < 0) {
		LOG_ERR("Could not get temperature: %d\n", ret);
	}

	return value.val1 * 100 + value.val2 / 10000;
}

static void broadcast_temperature()
{
	struct net_pkt *pkt;
	struct net_buf *buf;
	struct TemperatureMeasurementMsg message = {
		.cherry_mote_id = get_id() & ~IEEE802154_AR_FLAG_SET, // Need to disable bit that requires ACK.
		.sequential_no = global_sequence_number++,
		.timestamp = k_cycle_get_32(),
		.temperature_centigrades_celsius = get_temperature()
	};

	LOG_INF("Broadcasting temperature. CherryMote id: %u, seq: %u, timestamp: %u, temp: %d",
		message.cherry_mote_id,
		message.sequential_no,
		message.timestamp,
		message.temperature_centigrades_celsius);

	pkt = net_pkt_alloc_with_buffer(NULL, sizeof(message),
							AF_UNSPEC, 0,
							K_NO_WAIT);
	if (!pkt) {
		LOG_ERR("No more buffers");
		return;
	}

	buf = net_buf_frag_last(pkt->buffer);
	net_buf_add_mem(buf, (char *)&message, sizeof(message));

	LOG_DBG("Sending pkt %p buf %p", pkt, buf);
	LOG_HEXDUMP_DBG(buf->data, buf->len, "DATA >");

	int ret = radio_api->tx(ieee802154_dev, IEEE802154_TX_MODE_CSMA_CA,
				pkt, buf);
	if (ret) {
		LOG_ERR("Error while sending data: 0x%x", ret);
	} else {
		LOG_DBG("Temperature sent successfully");
	}
	net_pkt_unref(pkt);
}

static int cmd_transmit_temperature (const struct shell *sh, size_t argc, char **argv) {
	LOG_INF("Transmit temperature cmd");
	shell_print(sh, "uart lite start argc = %zd", argc);
	broadcast_temperature();
	return 0;
}

static int cmd_print_temperature (const struct shell *sh, size_t argc, char **argv) {
	if (last_received_temperature.temperature_centigrades_celsius == 0xffffffff) {
		shell_print(sh, "No temperature measurement received yet");
		return 0;
	}
	shell_print(sh, "Latest received measurement: CherryMote id=0x%x, seq: %u, timestamp: %u, temp: %d",
		last_received_temperature.cherry_mote_id,
		last_received_temperature.sequential_no,
		last_received_temperature.timestamp,
		last_received_temperature.temperature_centigrades_celsius);

	return 0;
}

static int cmd_toggle_led(const struct shell *sh, size_t argc, char **argv) {
	int ret = gpio_pin_toggle_dt(&led);
	if (ret < 0) {
		LOG_ERR("Could not toggle LED: %d\n", ret);
	}

	return 0;
}

static int cmd_spam_logs(const struct shell *sh, size_t argc, char **argv) {
	if (argc != 3) {
		LOG_ERR("Invalid number of arguments");
	}

	int message_count = strtol(argv[1], NULL, 10);
	int delay_ms = strtol(argv[2], NULL, 10);

	for (int i = 0; i < message_count; i++) {
		LOG_INF("Logs spammer: %d / %d", i, message_count);
		k_msleep(delay_ms);
	}

	return 0;
}


SHELL_STATIC_SUBCMD_SET_CREATE(sub_demo,
	SHELL_CMD_ARG(transmit, NULL, "Broadcast temperature", cmd_transmit_temperature, 1, 0),
	SHELL_CMD_ARG(print, NULL, "Print last received temperature", cmd_print_temperature, 1, 0),
	SHELL_CMD_ARG(toogle_led, NULL, "Toggles a LED", cmd_toggle_led, 1, 0),
	SHELL_CMD_ARG(spam_logs, NULL, "Sends $1 logs messages with $2ms intervals", cmd_spam_logs, 3, 0),
	SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_CMD_REGISTER(demo, &sub_demo, "Log test", NULL);

static bool init_ieee802154(void)
{
	LOG_INF("Initialize ieee802.15.4");

	if (!device_is_ready(ieee802154_dev)) {
		LOG_ERR("IEEE 802.15.4 device not ready");
		return false;
	}

	radio_api = (struct ieee802154_radio_api *)ieee802154_dev->api;

	radio_api->set_txpower(ieee802154_dev, 5);

	LOG_INF("Set channel %u", IEEE802_15_4_CHANNEL);
	radio_api->set_channel(ieee802154_dev, IEEE802_15_4_CHANNEL);

	/* Start ieee802154 */
	radio_api->start(ieee802154_dev);

	return true;
}

int net_recv_data(struct net_if *iface, struct net_pkt *pkt)
{
	const struct net_buf *buf = net_buf_frag_last(pkt->buffer);
 	LOG_INF("Received pkt %p, len %d, buf %p, buf len %d", pkt, net_pkt_get_len(pkt), buf, buf->len);
	LOG_HEXDUMP_INF(buf->data, buf->len, "DATA >");

	if (net_pkt_get_len(pkt) == (sizeof(last_received_temperature) + SUFFIX_SIZE)) {
		memcpy(&last_received_temperature, buf->data, sizeof(last_received_temperature));
	} else {
		LOG_INF("Frame length invalid - not possible to interpet as TemperatureMeasurementMsg");
	}

	net_pkt_unref(pkt);
 	return 0;
}


int main(void)
{
	LOG_INF("Starting 1KT interop demo");

	if (!gpio_is_ready_dt(&led)) {
		LOG_ERR("LED is not ready");
		return 0;
	}

	if (gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE) < 0) {
		LOG_ERR("Can't configure LED");
		return 0;
	}

	if (!init_ieee802154()) {
		LOG_ERR("Unable to initialize ieee802154");
		return 0;
	}

	if (!device_is_ready(thermometer)) {
		LOG_ERR("Thermometer %s is not ready\n", thermometer->name);
		return 0;
	}

	LOG_INF("Temperature device is %p, name is %s\n", thermometer, thermometer->name);

	return 0;
}
