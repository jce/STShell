/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file   fatfs.c
  * @brief  Code for fatfs applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
#include "fatfs.h"

uint8_t retUSER;    /* Return value for USER */
char USERPath[4];   /* USER logical drive path */
FATFS USERFatFS;    /* File system object for USER logical drive */
FIL USERFile;       /* File object for USER */

/* USER CODE BEGIN Variables */

/* USER CODE END Variables */

void MX_FATFS_Init(void)
{
  /*## FatFS: Link the USER driver ###########################*/
  retUSER = FATFS_LinkDriver(&USER_Driver, USERPath);

  /* USER CODE BEGIN Init */
  /* additional user code for init */
  /* USER CODE END Init */
}

/**
  * @brief  Gets Time from RTC
  * @param  None
  * @retval Time in DWORD
  */
DWORD get_fattime(void)
{
  /* USER CODE BEGIN get_fattime */
	union rv_u
	{
		struct as_date_s
		{
			int hsecond:5;
			int minute:6;
			int hour:5;
			int day:5;
			int month:4;
			int year:7;
		} as_date;
		DWORD as_dword;
	};
	union rv_u rv;
	RTC_TimeTypeDef t = {0};
	RTC_DateTypeDef d = {0};
	HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
	HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);
	rv.as_date.year = d.Year+20;			// Fat counts years from 1980.
	rv.as_date.month = d.Month;
	rv.as_date.day = d.Date;
	rv.as_date.hour = t.Hours;
	rv.as_date.minute = t.Minutes;
	rv.as_date.hsecond = t.Seconds / 2;		// Fat uses 2-second resolution.

	return rv.as_dword;

  /* USER CODE END get_fattime */
}

/* USER CODE BEGIN Application */

/* USER CODE END Application */
