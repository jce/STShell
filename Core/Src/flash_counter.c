/*
 * flash_counter.c
 *
 *  Created on: Sep 21, 2026
 *      Author: jeindhoven
 */
#include "flash_counter.h"
#include "usbd_storage_if.h"

#include <string.h>

// Counter struct.
typedef struct s_channel_counter
{
	uint16_t cnt;
	uint16_t tally[TALLY_HWORD_LEN];
} channel_counter;
// Lives in its own flash page.
channel_counter const *cnt_page = (channel_counter const *) CNT_LOC;

// Counts tallies per page number
uint32_t cnt_tally(uint32_t pagenr)
{
	uint32_t rv;
	for (rv = 0; rv < TALLY_HWORD_LEN; rv++)
	{
		if (cnt_page[pagenr].tally[rv] == 0xFFFF)
			break;
	}
	return rv;
}

// Consolidates all the tallies. And adds one to pagenum and the tally page.
void count_consolidate(uint32_t pagenum)
{
	uint16_t cnt_page_local[PAGE_NUM];
	for (int i = 0; i < PAGE_NUM; i++)
		cnt_page_local[i] = cnt_page[i].cnt + cnt_tally(i);
	cnt_page_local[PAGE_NUM-1] ++;				// Add one to the counter of the tally page
	cnt_page_local[pagenum] ++;					// Add one to the counter of the one parameter page
	HAL_FLASH_Unlock();
	if (HAL_OK != flash_erase_page(CNT_LOC))
	{
		printf("Error erasing flash page.\r\n");	// This erase is counted above, not here.
		HAL_FLASH_Lock();
		return;
	}
	for (int i = 0; i < PAGE_NUM; i++)
	{
		if (HAL_OK != HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, (uint32_t) cnt_page + 2 * (TALLY_HWORD_LEN + 1) * i  , cnt_page_local[i] ))
		{
			printf("Error programming flash.\r\n");
			HAL_FLASH_Lock();
			return;
		}
		for (int j = 0; j < TALLY_HWORD_LEN; j++)
			if (HAL_OK != HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, (uint32_t) cnt_page + 2 * ((TALLY_HWORD_LEN + 1) * i + 1 + j), 0xFFFF ))
			{
				printf("Error programming flash.\r\n");
				HAL_FLASH_Lock();
				return;
			}
	}
	HAL_FLASH_Lock();
}

void fc_count_page_erase(uint8_t pagenr)
{
	uint32_t tally = cnt_tally(pagenr);
	if (tally >= TALLY_HWORD_LEN)
		count_consolidate(pagenr);
	else
	{
		const uint16_t* flash_addr = &cnt_page[pagenr].tally[tally];
		HAL_FLASH_Unlock();
		if (HAL_OK != HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, (uint32_t) flash_addr, 0x0000 ))
			printf("Error programming flash.\r\n");
		HAL_FLASH_Lock();
	}
}

uint16_t get_page_counter(uint32_t pagenr)
{
	uint16_t rv =1;
	rv += cnt_page[pagenr].cnt;
	rv += cnt_tally(pagenr);
	return rv;
}

// Test for first boot after firmware update, and optionally increase flash counters
// for the flash used pages.
const uint16_t firstboot_flag __attribute__((section(".firstbootflag"), used)) = 0xFFFF;
void count_firmware_downloads(void)
{
	const uint16_t* p = &firstboot_flag;

	//if (firstboot_flag == 0xFFFF)	// This does not work, constant folding.
	if (*p == 0xFFFF)
	{
		HAL_FLASH_Unlock();
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, (uint32_t) p, 0x0000 );
		HAL_FLASH_Lock();
		uint32_t last_page = ((uint32_t) &firstboot_flag - FLASH_BASE) / PAGE_SIZE;
		for (uint32_t page = 0; page <= last_page; page++)
			fc_count_page_erase(page);
	}
}


