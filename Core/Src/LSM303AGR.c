/*
 * LSM303AGR.c
 *
 *  Created on: Sep 17, 2026
 *      Author: jeindhoven
 */

#include "LSM303AGR.h"
#include "main.h"
#include "shell.h"

// Init de LSM303AGR
void init_LSM303AGR()
{
	uint8_t reg[2];
	reg[0] = TEMP_CFG_REG_A;reg[1] = 0b11000000; HAL_I2C_Master_Transmit(&hi2c1, LSM303AGR_ADDR_A, reg, 2, HAL_MAX_DELAY);
	reg[0] = CTRL_REG1_A; 	reg[1] = 0b01010111; HAL_I2C_Master_Transmit(&hi2c1, LSM303AGR_ADDR_A, reg, 2, HAL_MAX_DELAY);
	reg[0] = CTRL_REG4_A; 	reg[1] = 0b10000000; HAL_I2C_Master_Transmit(&hi2c1, LSM303AGR_ADDR_A, reg, 2, HAL_MAX_DELAY);
}

float LSM303AGR_get_temp()
{
	HAL_StatusTypeDef rv;
	uint8_t reg[2];
	HAL_I2C_Mem_Read(&hi2c1, LSM303AGR_ADDR_A, OUT_TEMP_L_A | 0x80, I2C_MEMADD_SIZE_8BIT, reg, 2, 100);
	int16_t raw = (int16_t)((reg[1] << 8) | reg[0]);
	return raw / 256.0f + 25.0f;

}
