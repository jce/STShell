/*
 * nvstore.c
 *
 *  Created on: Sep 26, 2026
 *      Author: jeindhoven
 *
 *      Takes a single flash page and stores the last version
 *      of each descriptor, to be retrieved by nvfind.
 */

#include <string.h>

#include "flash_counter.h"
#include "nvstore.h"
#include "usbd_storage_if.h"

extern uint32_t _nvstore_start;
extern uint32_t _nvstore_end;

#define NV_START ((uint32_t)&_nvstore_start)
#define NV_END   ((uint32_t)&_nvstore_end)
#define NV_LEN   (NV_END - NV_START)
//#define NV_LEN		2048

uint32_t nvfree();
HAL_StatusTypeDef nvconsolidate(nvstore_desc desc, uint8_t size, void *p);

typedef union header{
		uint16_t as_word;
		struct {
			uint8_t desc;
			uint8_t size;
		};
	} header_t;

// Store a bit of information in NV
HAL_StatusTypeDef nvstore(nvstore_desc desc, uint8_t size, void *p)
{
	if (size > 254)						// Rounding 255 upwards would turn into 0.
		return HAL_ERROR;

	void* current = nvfind(desc);		// Check if the flash contents are set correctly already.
	if (current && memcmp(current, p, size) == 0)
		return HAL_OK;

	size = (size+1) & 0xFE;				// Size needs to be a multiple of halfwords. Round up.
	uint32_t free = nvfree();
	if (free < size+2)
		return nvconsolidate(desc, size, p);
	uint32_t writep = NV_START + free - (size+2);
	header_t header;
	header.desc = desc;
	header.size = size;

	HAL_FLASH_Unlock();
	HAL_StatusTypeDef rv = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, writep, header.as_word );
	if (HAL_OK != rv)
	{
		HAL_FLASH_Lock();
		return rv;
	}
	writep += 2;
	for (int i = 0; i < size; i += 2)
	{
		rv =  HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, writep+i, * (uint16_t*) ((uint8_t*)p+i) );
		if (HAL_OK != rv)
		{
			HAL_FLASH_Lock();
			return rv;
		}
	}
	HAL_FLASH_Lock();
	return HAL_OK;
}

// Layout:
// [desc][len][content 0 ... n-1]

// Find the NV location for the descriptor.
void* nvfind(nvstore_desc desc)
{
	uint8_t* p = (uint8_t*) NV_START;
	uint8_t len;
	while (p < (uint8_t*) (NV_START + NV_LEN))
	{
		if (*p == NV_UNUSED)
		{
			p += 2;
			continue;
		}
		len = *(p+1);
		if (*p != desc)
		{
			p += 2 + len;
			continue;
		}
		return p+2;
	}
	return NULL;
}

// Count the free length
uint32_t nvfree()
{
	uint8_t* p = (uint8_t*) NV_START;
	while (p < (uint8_t*) NV_END)
	{
		if (*p == NV_UNUSED)
		{
			p += 2;
			continue;
		}
		break;
	}
	return (uint32_t) p - NV_START;
}

// Consolidate
HAL_StatusTypeDef nvconsolidate(nvstore_desc desc, uint8_t size, void *p)
{
	// Using a static buffer for consolidation means risk of race conditions.
	static uint8_t buf[FLASH_PAGE_SIZE] __attribute__((aligned(8)));// FLASH_PAGE_SIZE IS NV_LEN, but NV_LEN is not constant...
	uint32_t remaining = NV_LEN;
	uint32_t len = 0;
	void *src;

	for (int i = 0; i < NV_UNUSED; i++)
	{
		src = nvfind(i);
		if (src)
			len = * ((uint8_t*) src-1);
		if (i == desc)								// New value of this record wins over flash value.
		{
			len = size;
			src = p;
		}
		if (src)
		{
			if ((len+2) > remaining)
				return HAL_ERROR;
			remaining -= (len+2);

			buf[remaining+0] = i;
			buf[remaining+1] = len;
			for (int j = 0; j < len; j++)
				buf[remaining+2+j] = * ((uint8_t*) src + j);
		}
	}

	for (int i = 0; i < remaining; i++)				// Fill the remaining space
		buf[i] = 0xFF;								// with 0xFF

	HAL_FLASH_Unlock();
	if (HAL_OK != flash_erase_page(NV_START))
	{
		printf("Error erasing flash page.\r\n");
		HAL_FLASH_Lock();
		return HAL_ERROR;
	}

	for (int i = 0; i < NV_LEN / 8; i++)
		if (HAL_OK != HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, (uint32_t) NV_START + 8 * i  , * (uint64_t*) &buf[8*i] ))
		{
			printf("Error programming flash.\r\n");
			HAL_FLASH_Lock();
			return HAL_ERROR;
		}

	HAL_FLASH_Lock();
													// fc_count_page_erase() has its own flash unlock / lock.
	fc_count_page_erase((NV_START - FLASH_BASE) / PAGE_SIZE);

	return HAL_OK;
}


