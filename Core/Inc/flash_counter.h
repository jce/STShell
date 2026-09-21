/*
 * flash_counter.h
 *
 *  Created on: Sep 21, 2026
 *      Author: jeindhoven
 */

#ifndef INC_FLASH_COUNTER_H_
#define INC_FLASH_COUNTER_H_

#include "main.h"

extern uint8_t _counter_start;
#define PAGE_SIZE 		0x800
#define PAGE_NUM		0x80
#define CNT_LOC			((uint32_t) &_counter_start)
#define TALLY_HWORD_LEN	7	// There is a limitation that you can write each halfword only once per erase cycle.
							// 128 pages x 7 tallies (16 bytes total) = 2048 bytes.

void fc_count_page_erase(uint8_t pagenr);
uint16_t get_page_counter(uint32_t pagenr);

#endif /* INC_FLASH_COUNTER_H_ */
