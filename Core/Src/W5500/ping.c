/*
 * ping.c
 *
 *  Created on: Oct 12, 2026
 *      Author: jeindhoven
 *      Well, most of this code came from Mistral!
 *
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "ping.h"
#include "w5500.h"
#include "socket.h"
#include "wizchip_conf.h"
#include "stdio.h"
#include "cmsis_os2.h"
#include "stm32f3xx_hal.h"

#define PING_SOCK			7
#define PING_ID				0x1234
#define PING_DATA_LEN		32
#define PING_TIMEOUT_MS		1000
#define PING_INTERVAL_MS	1000
#define PING_MAX_COUNT		256

#define PING_REQUEST		8
#define PING_REPLY			0
#define ICMP_ECHO_HEADER	8

typedef struct __attribute__((packed)) pingmsg_s
{
    uint8_t  Type;      // 8 = Echo Request, 0 = Echo Reply
    uint8_t  Code;      // altijd 0
    int16_t  CheckSum;
    int16_t  ID;        // identification
    int16_t  SeqNum;    // sequence number
    int8_t   Data[32];  // ping data (max 1452 = IPRAW-MTU − header)
} pingmsg;

/* ---- ICMP checksum: 16-bit one's complement over het bericht ----
 * Ontbreekt in de app note. Klassieke RFC 1071-implementatie.
 */
static uint16_t icmp_checksum(const uint8_t *buf, uint16_t len)
{
    uint32_t sum = 0;
    while (len > 1) {
        sum += (uint16_t)(buf[0] << 8 | buf[1]);   /* big-endian paren */
        buf += 2; len -= 2;
    }
    if (len) sum += (uint16_t)(buf[0] << 8);       /* oneven: pad met 0 */
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)~sum;
}

static int ping_parse_ip(const char *s, uint8_t out[4])
{
    uint16_t v[4];
    if (sscanf(s, "%hu.%hu.%hu.%hu", &v[0], &v[1], &v[2], &v[3]) != 4)
        return -1;
    for (int i = 0; i < 4; i++)
        if (v[i] > 255) return -1;
    for (int i = 0; i < 4; i++) out[i] = (uint8_t)v[i];
    return 0;
}

/**
    Swap the byte order of 16bit(short) wide variable.

    @param i 16bit value to swap
    @return Swapped value
*/
uint16_t swaps(uint16_t i) {
    uint16_t ret = 0;
    ret = (i & 0xFF) << 8;
    ret |= ((i >> 8) & 0xFF);
    return ret;
}

/**
    htons function converts a unsigned short from host to TCP/IP network byte order (which is big-endian).

    @param hostshort The value to convert.
    @return The value in TCP/IP network byte order.
*/
uint16_t htons(uint16_t hostshort) {
#ifdef SYSTEM_LITTLE_ENDIAN
    return swaps(hostshort);
#else
    return hostshort;
#endif
}

/**
    ntohs function converts a unsigned short from TCP/IP network byte order
    to host byte order (which is little-endian on Intel processors).

    @param netshort The value to convert.
    @return A 16-bit number in host byte order
*/
uint16_t ntohs(uint16_t netshort) {
#ifdef SYSTEM_LITTLE_ENDIAN
    return htons(netshort);
#else
    return netshort;
#endif
}

/* ---- Eén request versturen (app note §3.1, Example 3.1.2) ---- */
static void ping_send(uint8_t dest[4], uint16_t seq)
{
    pingmsg req;
    memset(&req, 0, sizeof(req));
    req.Type    = PING_REQUEST;
    req.Code    = 0;
    req.ID      = htons(PING_ID);
    req.SeqNum  = htons(seq);
    for (int i = 0; i < PING_DATA_LEN; i++)        /* patroon: net als de note, te vergelijken in reply */
        req.Data[i] = (uint8_t)(i & 0xFF);
    req.CheckSum = 0;
    req.CheckSum = htons(icmp_checksum((uint8_t *)&req, sizeof(req)));

    /* poortparameter = 3000 zoals de note; chip negeert 'm in IPRAW */
    sendto(PING_SOCK, (uint8_t *)&req, sizeof(req), dest, 3000);
}

static int ping_wait_reply(uint32_t start_tick, uint16_t seq, uint32_t *t1)
{
	uint8_t buf[sizeof(pingmsg) + 8 + 20]; // Ping message + ipraw info + marge.
	int16_t len;
	uint16_t port;
	uint8_t ip[4];

	while ((HAL_GetTick() - start_tick) < PING_TIMEOUT_MS)
	{
		*t1 = HAL_GetTick();
		uint16_t rlen = getSn_RX_RSR(PING_SOCK);

		if (rlen == 0)
		{
			osDelay(1);
			continue;
		}
		len = recvfrom(PING_SOCK, buf, sizeof(buf), ip, &port);
		if (len < ICMP_ECHO_HEADER)
			continue;
		pingmsg *ping = (pingmsg*) buf;
		if (ping->Type != PING_REPLY)
			continue;

		uint16_t ck = icmp_checksum(buf, len);
		if (ck != 0)
			continue;

		if (ntohs(ping->ID) == PING_ID && ntohs(ping->SeqNum) == seq)
			return 0;
	}
	return -1;
}

void s_ping(shell_ctx_t *ctx, int argc, char **argv)
{
	uint8_t dest[4];
	int count = 4;

	if (argc < 2 || ping_parse_ip(argv[1], dest) != 0)
	{
		shell_printf(ctx, "Usage: ping <a.b.c.d> [count]\r\n");
		return;
	}
	if (argc >=3)
	{
		count = atoi(argv[2]);
		if (count < 1)
			count = 1;
		if (count > PING_MAX_COUNT)
			count = PING_MAX_COUNT;
	}

	close(PING_SOCK);	// Just in case
	WIZCHIP_WRITE(Sn_PROTO(PING_SOCK), IPPROTO_ICMP);
	if (socket(PING_SOCK, Sn_MR_IPRAW, 0, 0) != PING_SOCK)
	{
		shell_printf(ctx, "Socket open failed\r\n");
		return;
	}
	while (getSn_SR(PING_SOCK) != SOCK_IPRAW)
	{
		osDelay(1);	// Wait for the socket to be open.
	}

	int sent = 0, recv_ok = 0;
	for (uint16_t seq = 0; seq < count; seq++)
	{
		ping_send(dest, seq);
		uint32_t t0 = HAL_GetTick();
		sent++;

		uint32_t t1;
		if (ping_wait_reply(t0, seq, &t1) == 0)
		{
			uint32_t rtt = t1 - t0;
			shell_printf(ctx, "reply from %u.%u.%u.%u: seq=%u time=%lu ms\r\n",
					dest[0], dest[1], dest[2], dest[3], seq, rtt);
			recv_ok++;
			if (ctx->flush)
				ctx->flush(ctx->flush_ctx);
		}
		else
		{
			shell_printf(ctx, "timeout: seq=%u\r\n", seq);
			if (ctx->flush)
				ctx->flush(ctx->flush_ctx);
		}

		if (seq + 1 < count)									// Delay to next send-ping-time.
			while ((HAL_GetTick() - t0) < PING_INTERVAL_MS)
			{
				osDelay(1);
			}
	}
	close(PING_SOCK);
	shell_printf(ctx, "--- %d sent, %d recv, %d%% loss ---\r\n",
			sent, recv_ok, (int)(100 * (sent-recv_ok) / sent));
}
