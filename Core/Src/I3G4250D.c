/*
 * I3G4250D.c
 *
 *  Created on: Sep 25, 2026
 *      Author: jeindhoven
 */

#include "I3G4250D.h"

struct S_I3G4250D i3g4250d;

#define EXIT_FAILURE \
		{ \
			SPI1_CS_HIGH(); \
			return HAL_ERROR; \
		}

#define VERIFY(REG, VAL) \
	{ \
		uint8_t buf[1]; \
		buf[0] = REG | 0x80; \
		SPI1_CS_LOW(); \
		if (HAL_OK != HAL_SPI_Transmit(&hspi1, buf, 1, SPI1_TIMEOUT)) \
			EXIT_FAILURE \
		if (HAL_OK != HAL_SPI_Receive(&hspi1, buf, 1, SPI1_TIMEOUT)) \
			EXIT_FAILURE \
		if (buf[0] != VAL) \
			EXIT_FAILURE \
		SPI1_CS_HIGH(); \
	}

#define CONF(REG, VAL) \
	{ \
		uint8_t buf[2] = {REG, VAL}; \
		SPI1_CS_LOW(); \
		if (HAL_OK != HAL_SPI_Transmit(&hspi1, buf, 2, SPI1_TIMEOUT)) \
			EXIT_FAILURE \
		SPI1_CS_HIGH(); \
		VERIFY(REG, VAL) \
	}

HAL_StatusTypeDef init_I3G4250D(void)
{
	VERIFY(WHO_AM_I, WHO_AM_I_VALUE);
	CONF(CTRL_REG1, 0b00001111);		// Data rate 100Hz, 12.5 Hz cutoff, enable x, y, z axes.
	CONF(CTRL_REG2, 0b00000000);		// No high pass filter(s).
	CONF(CTRL_REG3, 0b00000000);		// No interrupts.
	CONF(CTRL_REG4, 0b00000000);		// Little endian, full scale = 245 dps, no self test, 4 wire SPI.
	CONF(CTRL_REG5, 0b00000000);		// No boot, no FIFO, no high pass, int_sel = LPF1, out_sel = LPF1,
	return HAL_OK;
}

#undef EXIT_FAILURE
#define EXIT_FAILURE \
		{ \
			SPI1_CS_HIGH(); \
			I3G4250D_state = I3GERR; \
			return; \
		}
static uint8_t I3G4250D_buf[8];
enum I3G4250D_STATE {I3GIDLE, I3GREQ, I3GRECV, I3GDONE, I3GERR} I3G4250D_state = I3GIDLE;

void I3G4250D_10ms_int(void)
{
	if (I3G4250D_state == I3GIDLE || I3G4250D_state == I3GDONE || I3G4250D_state == I3GERR)
	{
		SPI1_CS_HIGH();
		I3G4250D_buf[0] = OUT_TEMP | 0x80 | 0x40;	// Multi read from 0x26
		SPI1_CS_LOW();
		I3G4250D_state = I3GREQ;
		if (HAL_OK != HAL_SPI_Transmit_IT(&hspi1, I3G4250D_buf, 1))
			EXIT_FAILURE;
	}
}

void I3G4250D_SPI1_Callback(void)
{
	if (I3G4250D_state == I3GREQ)
	{
		I3G4250D_state = I3GRECV;
		if (HAL_OK != HAL_SPI_Receive_IT(&hspi1, I3G4250D_buf, 8))
			EXIT_FAILURE;
		return;
	}
	if (I3G4250D_state == I3GRECV)
	{
		SPI1_CS_HIGH();
		I3G4250D_state = I3GDONE;
		i3g4250d.temp = I3G4250D_buf[0];
		i3g4250d.x = * (int16_t*) &I3G4250D_buf[2];
		i3g4250d.y = * (int16_t*) &I3G4250D_buf[4];
		i3g4250d.z = * (int16_t*) &I3G4250D_buf[6];
	}
}

void I3G4250D_SPI1_Failed(void)
{
	EXIT_FAILURE;
}

