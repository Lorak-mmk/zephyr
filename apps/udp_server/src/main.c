#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(udp_server, LOG_LEVEL_DBG);

#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>

#include <zephyr/shell/shell.h>

#define LISTEN_PORT 4242
#define TEMP_TRANSFER_INTERVAL_SECS 5

struct TemperatureMeasurementMsg {
    uint32_t cherry_mote_id;
    uint32_t sequential_no;
    uint32_t timestamp;
    int32_t temperature_centigrades_celsius;
};

static const struct device *const thermometer = 
	DEVICE_DT_GET(DT_ALIAS(ambient_temp0));


static uint32_t global_sequence_number;

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

void process_clients(int socket) {
	struct sockaddr_in client_addr;
	socklen_t client_addr_length = sizeof(client_addr);
	char buffer[24];
	while (true) {
		LOG_INF("Waiting for client connection");
		ssize_t received_bytes = zsock_recvfrom(socket, &buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &client_addr_length);
		if (received_bytes < 0) {
			LOG_ERR("Could not receive data from socket: %d", received_bytes);
			continue;
		}
		LOG_HEXDUMP_INF(buffer, sizeof(buffer), "Received: ");
		struct TemperatureMeasurementMsg message = {
			.cherry_mote_id = get_id(), // Need to disable bit that requires ACK.
			.sequential_no = global_sequence_number++,
			.timestamp = k_cycle_get_32(),
			.temperature_centigrades_celsius = get_temperature()
		};
		LOG_INF("Responding with temperature. CherryMote id: %u, seq: %u, timestamp: %u, temp: %d",
			message.cherry_mote_id,
			message.sequential_no,
			message.timestamp,
			message.temperature_centigrades_celsius);

		int result = zsock_sendto(socket, &message, sizeof(message), 0, (struct sockaddr *)&client_addr, client_addr_length);
		if (result < 0) {
			LOG_ERR("Could not send data to socket: %d", result);
		}
	}
}

int main(void)
{
	LOG_INF("Starting udp temperature server interop demo");

	if (!device_is_ready(thermometer)) {
		LOG_ERR("Thermometer %s is not ready\n", thermometer->name);
		return 0;
	}

	LOG_INF("Temperature device is %p, name is %s\n", thermometer, thermometer->name);

	int socket = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (socket < 0) {
		LOG_ERR("Can't create socket %d\n", socket);
		return 0;
	}

	struct sockaddr_in addr4;
	(void)memset(&addr4, 0, sizeof(addr4));
	addr4.sin_family = AF_INET;
	addr4.sin_port = htons(LISTEN_PORT);
	int ret = zsock_bind(socket, (struct sockaddr *)&addr4, sizeof(addr4));
	if (ret < 0) {
		LOG_ERR("Can't bind socket %d\n", ret);
	}

	process_clients(socket);

	return 0;
}
