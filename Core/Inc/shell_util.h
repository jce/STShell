/*
 * shell_util.h
 *
 *  Created on: Sep 13, 2026
 *      Author: jeindhoven
 */

#ifndef INC_SHELL_UTIL_H_
#define INC_SHELL_UTIL_H_

void shell_tx_hex(uint32_t val);
void s_rd_hex_line(uint32_t addr, uint8_t *data, uint32_t len);
uint32_t s_atoi_hex(const char *s);

#endif /* INC_SHELL_UTIL_H_ */
