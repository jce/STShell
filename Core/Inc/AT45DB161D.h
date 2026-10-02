/*
 * AT45DB161D.h
 *
 *  Created on: Sep 30, 2026
 *      Author: jeindhoven
 */

#ifndef INC_AT45DB161D_H_
#define INC_AT45DB161D_H_

#include <stdint.h>
#include "stm32f3xx_hal.h"

#define SPI2_TIMEOUT								100

// Read commands
#define Main_Memory_Page_Read						0xD2
#define ontinuous_Array_Read_Legacy_Command 		0xE8
#define Continuous_Array_Read_Low_Frequency			0x03
#define Continuous_Array_Read_High_Frequency		0x0B
#define Buffer_1_Read_Low_Frequency					0xD1
#define Buffer_2_Read_Low_Frequency					0xD3
#define Buffer_1_Read								0xD4
#define Buffer_2_Read								0xD6

// Program and erase commands
#define Buffer_1_Write								0x84
#define Buffer_2_Write								0x87
#define Buffer_1_to_Main_Memory_Page_Program_with_Builtin_Erase		0x83
#define Buffer_2_to_Main_Memory_Page_Program_with_Builtin_Erase		0x86
#define Buffer_1_to_Main_Memory_Page_Program_without_Builtin_Erase	0x88
#define Buffer_2_to_Main_Memory_Page_Program_without_Builtin_Erase	0x89
#define Page_Erase									0x81
#define Block_Erase									0x50
#define Sector_Erase								0x7C
#define Chip_Erase									0xC7, 0x94, 0x80, 0x9A
#define Main_Memory_Page_Program_Through_Buffer_1	0x82
#define Main_Memory_Page_Program_Through_Buffer_2	0x85

// Protection and security commands
#define Enable_Sector_Protection					0x3D, 0x2A, 0x7F, 0xA9
#define Disable_Sector_Protection					0x3D, 0x2A, 0x7F, 0x9A
#define Erase_Sector_Protection_Register			0x3D, 0x2A, 0x7F, 0xCF
#define Program_Sector_Protection_Register			0x3D, 0x2A, 0x7F, 0xFC
#define Read_Sector_Protection_Register				0x32
#define Sector_Lockdown								0x3D, 0x2A, 0x7F, 0x30
#define Read_Sector_Lockdown_Register				0x35
#define Program_Security_Register					0x9B, 0x00, 0x00, 0x00
#define Read_Security_Register						0x77

// Additional Commands
#define Main_Memory_Page_to_Buffer_1_Transfer		0x53
#define Main_Memory_Page_to_Buffer_2_Transfer		0x55
#define Main_Memory_Page_to_Buffer_1_Compare		0x60
#define Main_Memory_Page_to_Buffer_2_Compare		0x61
#define Auto_Page_Rewrite_through_Buffer_1			0x58
#define Auto_Page_Rewrite_through_Buffer_2			0x59
#define Deep_Power_down								0xB9
#define Resume_from_Deep_Power_down					0xAB
#define Status_Register_Read						0xD7
#define Manufacturer_and_Device_ID_Read				0x9F

// Legacy Commands
#define Buffer_1_Read_Legacy						0x54
#define Buffer_2_Read_Legacy						0x56
#define Main_Memory_Page_Read_Legacy				0x52
#define Continuous_Array_Read_Legacy				0x68
#define Status_Register_Read_Legacy					0x57

uint8_t at45_status(void);								// Returns the status page.
HAL_StatusTypeDef at45_read_page(uint8_t *buf, uint16_t pageaddr);	// Reads 512 byte page.
HAL_StatusTypeDef at45_write_page(uint8_t *buf, uint16_t pageaddr);	// Reads 512 byte page.
HAL_StatusTypeDef at45_read_page_ll(uint8_t *buf, uint16_t pageaddr);
HAL_StatusTypeDef at45_read_page_dma(uint8_t *buf, uint16_t pageaddr);
void HAL_SPI_RxCpltCallback_SPI2();

#define AT_NUM_PAGES 								4096
#define AT_RESERVED_PAGES 							256
#define AT_PAGE_SIZE								512

#endif /* INC_AT45DB161D_H_ */
