/*
 * net.h
 *
 *  Created on: Oct 7, 2026
 *      Author: jeindhoven
 */

#ifndef INC_NET_H_
#define INC_NET_H_

#include "shell.h"
#define SPI3_TIMEOUT	100
void s_net(shell_ctx_t *ctx, int argc, char **argv);
void net_init(void);
void udpListen(void);
void init_shell_telnet(void);

#endif /* INC_NET_H_ */
