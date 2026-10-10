/*
 * nvstore.h
 *
 *  Created on: Sep 26, 2026
 *      Author: jeindhoven
 */

#ifndef INC_NVSTORE_H_
#define INC_NVSTORE_H_

#include "main.h"

// For each unique data source, add a descriptor here.
typedef enum nvstore_desc_e {
	NV_NOTE = 0x00,
	NV_MOTD = 0x01,
	NV_BOOTCOUNT = 0x02,
	NV_LSM303AGR_MAGCAL = 0x03,
	NV_PWMMODE = 0x04,
	NV_NET_IP = 0x05,
	NV_NET_GW = 0x06,
	NV_NET_MASK = 0x07,
	NV_NET_MAC = 0x08,
	NV_NET_DNS = 0x09,
	NV_UNUSED = 0xFF
} nvstore_desc;

HAL_StatusTypeDef nvstore(nvstore_desc desc, uint8_t size, void *p);	// Store a bit of information in NV
void* nvfind(nvstore_desc desc);					// Find the NV location for the descriptor.
void nvget(void*, nvstore_desc, uint32_t, void*);	// Fill in a field with nvstore variable, with default.

#endif /* INC_NVSTORE_H_ */
