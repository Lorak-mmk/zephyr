#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(udp_client, LOG_LEVEL_DBG);

#include <zephyr/drivers/uart.h>
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


void send_requests(int socket) {
	struct sockaddr_in server_addr;
	socklen_t server_addr_length = sizeof(server_addr);

	char buffer[24] = {0};
	while (true) {
		k_msleep(3000);
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(LISTEN_PORT);
		zsock_inet_pton(AF_INET, CONFIG_NET_CONFIG_PEER_IPV4_ADDR,
			  &server_addr.sin_addr);
		LOG_INF("Sending request");
		int ret = zsock_sendto(socket, &buffer, sizeof(buffer), 0, (struct sockaddr *)&server_addr, server_addr_length);
		if (ret < 0) {
			LOG_ERR("sendto failed (%d)", ret);
			continue;
		}

		LOG_INF("Waiting for response");
		ssize_t received_bytes = zsock_recvfrom(socket, &buffer, sizeof(buffer), 0, (struct sockaddr *)&server_addr, &server_addr_length);
		if (received_bytes < 0) {
			LOG_ERR("Could not receive data from socket: %d", received_bytes);
			continue;
		}
		if (received_bytes != sizeof(struct TemperatureMeasurementMsg)) {
			LOG_ERR("Received message has wrong size: %d", received_bytes);
			continue;
		}
		struct TemperatureMeasurementMsg *message = (struct TemperatureMeasurementMsg *)buffer;
		LOG_INF("Received temperature measurement. CherryMote id: %u, seq: %u, timestamp: %u, temp: %d",
			message->cherry_mote_id,
			message->sequential_no,
			message->timestamp,
			message->temperature_centigrades_celsius);
	}
}

int main(void)
{
	LOG_INF("Starting udp temperature server interop demo");

	int socket = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (socket < 0) {
		LOG_ERR("Can't create socket %d\n", socket);
		return 0;
	}

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 100000;

	int ret = zsock_setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &tv,sizeof(tv));
	if (ret != 0) {
		LOG_ERR("Can't set socket timeout %d\n", ret);
		return 0;
	}

	send_requests(socket);

	return 0;
}
