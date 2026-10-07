/*
 * net.c
 *
 *  Created on: Oct 7, 2026
 *      Author: jeindhoven
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "net.h"
#include "nvstore.h"
#include "main.h"
#include "shell.h"

#define LIMITED_V \
		( \
			v[0] <= 255 && \
			v[1] <= 255 && \
			v[2] <= 255 && \
			v[3] <= 255 && \
			v[4] <= 255 && \
			v[5] <= 255 \
		)

#define V_TO_DATA \
		data[0] = (uint8_t) v[0]; \
		data[1] = (uint8_t) v[1]; \
		data[2] = (uint8_t) v[2]; \
		data[3] = (uint8_t) v[3]; \
		data[4] = (uint8_t) v[4]; \
		data[5] = (uint8_t) v[5];

void net_print_status();

void s_net(int argc, char **argv)
{
	uint8_t data[6] = {0}; int v[6] = {0};
	if (argc == 3 && strcmp(argv[1], "ip") == 0 && sscanf(argv[2], "%u.%u.%u.%u", &v[0], &v[1], &v[2], &v[3]) == 4 && LIMITED_V)
	{
		V_TO_DATA;
		nvstore(NV_NET_IP, 4, data);
		// Program the IP in the W5500
		shell_printf("OK\r\n");
		return;
	}
	if (argc == 3 && strcmp(argv[1], "gw") == 0 && sscanf(argv[2], "%u.%u.%u.%u", &v[0], &v[1], &v[2], &v[3]) == 4 && LIMITED_V)
	{
		V_TO_DATA;
		nvstore(NV_NET_GW, 4, data);
		// Program the IP in the W5500
		shell_printf("OK\r\n");
		return;
	}
	if (argc == 3 && strcmp(argv[1], "mask") == 0 && sscanf(argv[2], "%u.%u.%u.%u", &v[0], &v[1], &v[2], &v[3]) == 4 && LIMITED_V)
	{
		V_TO_DATA;
		nvstore(NV_NET_MASK, 4, data);
		// Program the IP in the W5500
		shell_printf("OK\r\n");
		return;
	}
	if (argc == 3 && strcmp(argv[1], "mac") == 0 && sscanf(argv[2], "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) == 6 && LIMITED_V)
	{
		V_TO_DATA;
		nvstore(NV_NET_MAC, 6, data);
		// Program the IP in the W5500
		shell_printf("OK\r\n");
		return;
	}
	if (argc == 1)
	{
		net_print_status();
		return;
	}
	shell_tx_str("Usage:\r\n\r\n");
	shell_tx_str("ip [aaa.bbb.ccc.ddd] - Configures own IP address.\r\n");
	shell_tx_str("gw [aaa.bbb.ccc.ddd] - Configures gateway IP address.\r\n");
	shell_tx_str("mask [aaa.bbb.ccc.ddd] - Configures own netmask.\r\n");
	shell_tx_str("mac [aa:bb:cc:dd:ee:ff] - Configures own mac address. Hexadecimal input.\r\n");
}

void net_print_status(void)
{
	// Should work by querying the chip. For now, display by showing the config file.
	const uint8_t *data;
	const uint8_t empty[6] = {0};
	data = (uint8_t*) nvfind(NV_NET_IP);
	if (!data)
		data = empty;
	shell_printf("ip: %d.%d.%d.%d\r\n", data[0], data[1], data[2], data[3]);
	data = (uint8_t*) nvfind(NV_NET_GW);
	if (!data)
		data = empty;
	shell_printf("gateway: %d.%d.%d.%d\r\n", data[0], data[1], data[2], data[3]);
	data = (uint8_t*) nvfind(NV_NET_MASK);
	if (!data)
		data = empty;
	shell_printf("netmask: %d.%d.%d.%d\r\n", data[0], data[1], data[2], data[3]);
	data = (uint8_t*) nvfind(NV_NET_MAC);
	if (!data)
		data = empty;
	shell_printf("mac: %02x:%02x:%02x:%02x:%02x:%02x\r\n", data[0], data[1], data[2], data[3], data[4], data[5]);
}

void net_init(void)
{

}














