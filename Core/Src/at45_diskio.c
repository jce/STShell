/*
 * at45_diskio.c
 *
 *  Created on: Oct 2, 2026
 *      Author: jeindhoven
 */

#include "at45_diskio.h"
#include "diskio.h"

DSTATUS at45_disk_initialize (BYTE pdrv)
{
	return 0;
}

DSTATUS at45_disk_status (BYTE pdrv)
{
	return 0;
}

DRESULT at45_disk_read (BYTE pdrv, BYTE* buff, DWORD sector, UINT count)
{
	return RES_OK;
}

DRESULT at45_disk_write (BYTE pdrv, const BYTE* buff, DWORD sector, UINT count)
{
	return RES_OK;
}

DRESULT at45_disk_ioctl (BYTE pdrv, BYTE cmd, void* buff)
{
	return RES_OK;
}


#define BLOCK_SIZE 512 /* Block Size in Bytes */
/* Private variables ------------------------------------------------------*/
static volatile DSTATUS Stat = STA_NOINIT; /* Disk status */
