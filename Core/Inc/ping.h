/*
 * ping.h
 *
 *  Created on: Oct 12, 2026
 *      Author: jeindhoven
 */

#ifndef INC_PING_H_
#define INC_PING_H_

#include "shell.h"

#define Sn_PROTO(N) (_W5500_IO_BASE_ + (0x0014 << 8) + (WIZCHIP_SREG_BLOCK(N) << 3))	// Why o why is this missing in the library?

void s_ping(shell_ctx_t *ctx, int argc, char **argv);

#endif /* INC_PING_H_ */
