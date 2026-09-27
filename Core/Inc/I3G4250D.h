/*
 * I3G4250D.h
 *
 *  Created on: Sep 25, 2026
 *      Author: jeindhoven
 */

#ifndef INC_I3G4250D_H_
#define INC_I3G4250D_H_

#include "main.h"

#define SPI1_TIMEOUT		100

#define WHO_AM_I			0x0F
#define CTRL_REG1			0x20
#define CTRL_REG2			0x21
#define CTRL_REG3			0x22
#define CTRL_REG4			0x23
#define CTRL_REG5			0x24
#define REFERENCE_DATACAPTURE	0x25
#define OUT_TEMP			0x26
#define STATUS_REG			0x27
#define OUT_X_L				0x28
#define OUT_X_H				0x29
#define OUT_Y_L				0x2A
#define OUT_Y_H				0x2B
#define OUT_Z_L				0x2C
#define OUT_Z_H				0x2D
#define FIFO_CTRL_REG		0x2E
#define FIFO_SRC_REG		0x2F
#define INT1_CFG			0x30
#define INT1_SRC			0x31
#define INT1_THS_XH			0x32
#define INT1_THS_XL			0x33
#define INT1_THS_YH			0x34
#define INT1_THS_YL			0x35
#define INT1_THS_ZH			0x36
#define INT1_THS_ZL			0x37
#define INT1_DURATION		0x38

#define WHO_AM_I_VALUE		0b11010011

#define SPI1_CS_LOW()	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_RESET)
#define SPI1_CS_HIGH()	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_SET)

HAL_StatusTypeDef init_I3G4250D(void);
void I3G4250D_10ms_int(void);
void I3G4250D_SPI1_Callback(void);
void I3G4250D_SPI1_Failed(void);


struct S_I3G4250D {				// Let op, structure is niet atomic geschreven.
		float temp;					// Temperature in [degC]
		float x;
		float y;
		float z;
};
extern struct S_I3G4250D i3g4250d;

#endif /* INC_I3G4250D_H_ */
