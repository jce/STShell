/*
 * LSM303AGR.c
 *
 *  Created on: Sep 17, 2026
 *      Author: jeindhoven
 */

#include "LSM303AGR.h"
#include "main.h"
#include "shell.h"

#define VERIFY(DST, REG, VAL) \
	{	\
		HAL_StatusTypeDef rv; \
		uint8_t data; \
		rv = HAL_I2C_Mem_Read(&hi2c1, DST, REG, I2C_MEMADD_SIZE_8BIT, &data, 1, I2C_TIMEOUT); \
		if (rv != HAL_OK) \
			return rv; \
		if (data != VAL) \
			return HAL_ERROR; \
	}

#define CONF(DST, REG, VAL) \
	{ \
		HAL_StatusTypeDef rv; \
		uint8_t reg[2] = {REG, VAL}; \
		rv = HAL_I2C_Master_Transmit(&hi2c1, DST, reg, 2, I2C_TIMEOUT); \
		if (rv != HAL_OK) \
			return rv; \
		VERIFY(DST, REG, VAL) \
	}

// Init de LSM303AGR
HAL_StatusTypeDef init_LSM303AGR(void)
{
	VERIFY(LSM303AGR_ADDR_A, WHO_AM_I_A, WHO_AM_I_A_VALUE);
	VERIFY(LSM303AGR_ADDR_M, WHO_AM_I_M, WHO_AM_I_M_VALUE);
	CONF(LSM303AGR_ADDR_A, TEMP_CFG_REG_A, 0b11000000);
	CONF(LSM303AGR_ADDR_A, CTRL_REG1_A, 0b01010111);
	CONF(LSM303AGR_ADDR_A, CTRL_REG4_A, 0b10001000);
	CONF(LSM303AGR_ADDR_M, CFG_REG_A_M, 0b10000000);
	return HAL_OK;
}

// Mechanism to regularly fetch the sensor values
enum LSM303AGR_STATE {LSMIDLE, LSMTEMP, LSMACCEL, LSMMAG, LSMDONE, LSMERROR} LSM303AGR_state = LSMIDLE;
static uint8_t LSM303AGR_buf[6];	// Readout buffer
struct S_LSM303AGR lsm303agr;		// Result buffer
void LSM303AGR_10ms_int(void)
{
	if (! ( LSM303AGR_state == LSMDONE || LSM303AGR_state == LSMIDLE || LSM303AGR_state == LSMERROR))
		return;
	LSM303AGR_state = LSMTEMP;
	HAL_StatusTypeDef rv;

	rv = HAL_I2C_Mem_Read_IT(&hi2c1, LSM303AGR_ADDR_A, OUT_TEMP_L_A | 0x80, I2C_MEMADD_SIZE_8BIT, LSM303AGR_buf, 2);
	if (rv != HAL_OK)
		LSM303AGR_state = LSMERROR;
}

void LSM303AGR_I2C_Callback(void)
{
	HAL_StatusTypeDef rv;
	int16_t raw;
	if (LSM303AGR_state == LSMTEMP)
	{
		raw = (int16_t)((LSM303AGR_buf[1] << 8) | LSM303AGR_buf[0]);
		lsm303agr.temp = raw / 256.0f + 25.0f;

		LSM303AGR_state = LSMACCEL;
		rv = HAL_I2C_Mem_Read_IT(&hi2c1, LSM303AGR_ADDR_A, OUT_X_L_A | 0x80, I2C_MEMADD_SIZE_8BIT, LSM303AGR_buf, 6);
		if (rv != HAL_OK)
			LSM303AGR_state = LSMERROR;
		return;
	}

	if (LSM303AGR_state == LSMACCEL)
	{
		raw = (int16_t)((LSM303AGR_buf[1] << 8) | LSM303AGR_buf[0]);
		lsm303agr.acc.x = raw / 16.0f / 1024.0f;
		raw = (int16_t)((LSM303AGR_buf[3] << 8) | LSM303AGR_buf[2]);
		lsm303agr.acc.y = raw / 16.0f / 1024.0f;
		raw = (int16_t)((LSM303AGR_buf[5] << 8) | LSM303AGR_buf[4]);
		lsm303agr.acc.z = raw / 16.0f / 1024.0f;

		LSM303AGR_state = LSMMAG;
		rv = HAL_I2C_Mem_Read_IT(&hi2c1, LSM303AGR_ADDR_M, OUTX_L_REG_M | 0x80, I2C_MEMADD_SIZE_8BIT, LSM303AGR_buf, 6);
		if (rv != HAL_OK)
			LSM303AGR_state = LSMERROR;
		return;
	}

	if (LSM303AGR_state == LSMMAG)
	{
		raw = (int16_t)((LSM303AGR_buf[1] << 8) | LSM303AGR_buf[0]);
		lsm303agr.mag.x = raw * 0.0015f;
		raw = (int16_t)((LSM303AGR_buf[3] << 8) | LSM303AGR_buf[2]);
		lsm303agr.mag.y = raw * 0.0015f;
		raw = (int16_t)((LSM303AGR_buf[5] << 8) | LSM303AGR_buf[4]);
		lsm303agr.mag.z = raw * 0.0015f;

		LSM303AGR_state = LSMDONE;
		return;
	}
}

void LSM303AGR_I2C_Err_Callback(void)
{
	LSM303AGR_state = LSMERROR;
}

