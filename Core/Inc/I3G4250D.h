/*
 * I3G4250D.h
 *
 *  Created on: Sep 25, 2026
 *      Author: jeindhoven
 */

#ifndef INC_I3G4250D_H_
#define INC_I3G4250D_H_

#include "main.h"

#define SPI_TIMEOUT			100

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

HAL_StatusTypeDef init_I3G4250D(void);
//void LSM303AGR_10ms_int(void);
//void LSM303AGR_I2C_Callback(void);
//void LSM303AGR_I2C_Err_Callback(void);

struct S_I3G4250D {				// Let op, structure is niet atomic geschreven.
		float temp;					// Temperature in [degC]
		float x;
		float y;
		float z;
};
extern struct S_I3G4250D i3g4250d;

#endif /* INC_I3G4250D_H_ */
