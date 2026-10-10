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
#include "w5500.h"
#include "wizchip_conf.h"
#include "socket.h"


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

void net_print_status(shell_ctx_t *ctx);
void net_init_netinfo(void);
static void cmd_net_stat(shell_ctx_t *ctx);

void s_net(shell_ctx_t *ctx, int argc, char **argv)
{
	uint8_t data[6] = {0}; int v[6] = {0};
	if (argc == 3 && strcmp(argv[1], "mac") == 0 && sscanf(argv[2], "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) == 6 && LIMITED_V)
	{
		V_TO_DATA;
		nvstore(NV_NET_MAC, 6, data);
		net_init_netinfo();
		shell_printf(ctx, "OK\r\n");
		return;
	}
	if (argc == 3 && strcmp(argv[1], "ip") == 0 && sscanf(argv[2], "%u.%u.%u.%u", &v[0], &v[1], &v[2], &v[3]) == 4 && LIMITED_V)
	{
		V_TO_DATA;
		nvstore(NV_NET_IP, 4, data);
		net_init_netinfo();
		shell_printf(ctx, "OK\r\n");
		return;
	}
	if (argc == 3 && strcmp(argv[1], "mask") == 0 && sscanf(argv[2], "%u.%u.%u.%u", &v[0], &v[1], &v[2], &v[3]) == 4 && LIMITED_V)
	{
		V_TO_DATA;
		nvstore(NV_NET_MASK, 4, data);
		net_init_netinfo();
		shell_printf(ctx, "OK\r\n");
		return;
	}
	if (argc == 3 && strcmp(argv[1], "gw") == 0 && sscanf(argv[2], "%u.%u.%u.%u", &v[0], &v[1], &v[2], &v[3]) == 4 && LIMITED_V)
	{
		V_TO_DATA;
		nvstore(NV_NET_GW, 4, data);
		net_init_netinfo();
		shell_printf(ctx, "OK\r\n");
		return;
	}
	if (argc == 3 && strcmp(argv[1], "dns") == 0 && sscanf(argv[2], "%u.%u.%u.%u", &v[0], &v[1], &v[2], &v[3]) == 4 && LIMITED_V)
	{
		V_TO_DATA;
		nvstore(NV_NET_DNS, 4, data);
		net_init_netinfo();
		shell_printf(ctx, "OK\r\n");
		return;
	}
	if ((argc == 2 && strcmp(argv[1], "info") == 0) || (argc == 2 && strcmp(argv[1], "status") == 0) || (argc == 2 && strcmp(argv[1], "stat") == 0))
	{
		net_print_status(ctx);
		return;
	}
	shell_tx_str(ctx, "Usage:\r\n\r\n");
	shell_tx_str(ctx, "mac [aa:bb:cc:dd:ee:ff] - Configures own mac address. Hexadecimal input.\r\n");
	shell_tx_str(ctx, "ip [aaa.bbb.ccc.ddd] - Configures own IP address.\r\n");
	shell_tx_str(ctx, "mask [aaa.bbb.ccc.ddd] - Configures own netmask.\r\n");
	shell_tx_str(ctx, "gw [aaa.bbb.ccc.ddd] - Configures gateway IP address.\r\n");
	shell_tx_str(ctx, "dns [aaa.bbb.ccc.ddd] - Configures DNS address.\r\n");
	shell_tx_str(ctx, "info / stat / status - Show current status.\r\n");
}

void net_print_status(shell_ctx_t *ctx)
{
	wiz_NetInfo ni;
	wizchip_getnetinfo(&ni);
	shell_printf(ctx, "MAC    : %02X:%02X:%02X:%02X:%02X:%02X\r\n", ni.mac[0], ni.mac[1], ni.mac[2], ni.mac[3], ni.mac[4], ni.mac[5]);
	shell_printf(ctx, "IP     : %u.%u.%u.%u\r\n", ni.ip[0], ni.ip[1], ni.ip[2], ni.ip[3]);
	shell_printf(ctx, "Mask   : %u.%u.%u.%u\r\n", ni.sn[0], ni.sn[1], ni.sn[2], ni.sn[3]);
	shell_printf(ctx, "Gateway: %u.%u.%u.%u\r\n", ni.gw[0], ni.gw[1], ni.gw[2], ni.gw[3]);
	shell_printf(ctx, "DNS    : %u.%u.%u.%u\r\n", ni.dns[0], ni.dns[1], ni.dns[2], ni.dns[3]);
	cmd_net_stat(ctx);
}

static const char *sn_sr_name(uint8_t sr)
{
    switch (sr) {
    case SOCK_CLOSED:      return "CLOSED";
    case SOCK_INIT:        return "INIT";
    case SOCK_LISTEN:      return "LISTEN";
    case SOCK_SYNSENT:     return "SYNSENT";
    case SOCK_SYNRECV:     return "SYNRECV";
    case SOCK_ESTABLISHED: return "ESTABLISHED";
    case SOCK_FIN_WAIT:    return "FIN_WAIT";
    case SOCK_CLOSING:     return "CLOSING";
    case SOCK_TIME_WAIT:   return "TIME_WAIT";
    case SOCK_CLOSE_WAIT:  return "CLOSE_WAIT";
    case SOCK_LAST_ACK:    return "LAST_ACK";
    case SOCK_UDP:         return "UDP";
    case SOCK_MACRAW:      return "MACRAW";
    default:               return "???";
    }
}

static void cmd_net_stat(shell_ctx_t *ctx)
{
    shell_printf(ctx, "Sockets:\r\n");
    shell_printf(ctx, "nr  state         port  tx_used  rx_ready\r\n");
    for (uint8_t s = 0; s < 8; s++)
    {
        uint8_t  sr     = getSn_SR(s);       /* TODO: via net-taak, zie cris-afpraak */
        uint16_t port   = getSn_PORT(s);
        uint16_t txfree = getSn_TX_FSR(s);   /* vrije tx-ruimte */
        uint16_t rxrdy  = getSn_RX_RSR(s);   /* ontvangen, ongelezen bytes */
        uint16_t txsize = getSn_TXBUF_SIZE(s);   /* 2 = 2 KB */
        uint16_t rxsize = getSn_RXBUF_SIZE(s);

        shell_printf(ctx, "%2u  %-11s  %5u  %5u/%uK  %6u/%uK\r\n",
                     s, sn_sr_name(sr), port,
                     txsize*1024 - txfree, txsize,
                     rxrdy, rxsize);
    }
}

void cris_enter(void)
{
    __disable_irq();
}

void cris_exit(void)
{
    __enable_irq();
}

void spi_tx(uint8_t tx)
{
	HAL_SPI_Transmit(&hspi3, &tx, 1, SPI3_TIMEOUT);
}

uint8_t spi_rx(void)
{
	uint8_t rv;
	HAL_SPI_Receive(&hspi3, &rv, 1, SPI3_TIMEOUT);
	return rv;
}

void cs_low()
{
	HAL_GPIO_WritePin(SPI3_CS_GPIO_Port, SPI3_CS_Pin, GPIO_PIN_RESET);
}

void cs_high()
{
	HAL_GPIO_WritePin(SPI3_CS_GPIO_Port, SPI3_CS_Pin, GPIO_PIN_SET);
}

void net_init(void)
{
	// Feed functions to W5500 library
	reg_wizchip_spi_cbfunc(spi_rx, spi_tx);
	reg_wizchip_cs_cbfunc(cs_low, cs_high);
	reg_wizchip_cris_cbfunc(cris_enter, cris_exit);
	if (getVERSIONR() != 0x04) while(1) {}
	uint8_t txrxsize[8] = {2,2,2,2,2,2,2,2};
	wizchip_init(txrxsize, txrxsize);

	net_init_netinfo();
}

void net_init_netinfo(void)
{
	wiz_NetInfo netinfo = {0};
	uint8_t def[6] = {0};
	nvget(&netinfo.mac, NV_NET_MAC, 6, def);
	nvget(&netinfo.ip, NV_NET_IP, 4, def);
	nvget(&netinfo.sn, NV_NET_MASK, 4, def);
	nvget(&netinfo.gw, NV_NET_GW, 4, def);
	nvget(&netinfo.dns, NV_NET_DNS, 4, def);
	netinfo.dhcp = NETINFO_STATIC;
	wizchip_setnetinfo(&netinfo);
}












