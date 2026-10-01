/*
 * AT45DB161D.c
 *
 *  Created on: Sep 30, 2026
 *      Author: jeindhoven
 */

// Left the chip in factory default 528-bytes-per-page mode. Changing this is irreversible
// per flashchip, and requires changing the addressing scheme. Only the first 512 bytes are
// used now anyways.

#include "AT45DB161D.h"
#include "main.h"

#define SPI2_CS_LOW  HAL_GPIO_WritePin(SPI2_CS_GPIO_Port, SPI2_CS_Pin, GPIO_PIN_RESET);
#define SPI2_CS_HIGH HAL_GPIO_WritePin(SPI2_CS_GPIO_Port, SPI2_CS_Pin, GPIO_PIN_SET);

HAL_StatusTypeDef at45_read_page(uint8_t *buf, uint16_t pageaddr)
{
	HAL_StatusTypeDef rv;
	uint8_t txdata[8] = {0};
	txdata[0] = Main_Memory_Page_Read;
	txdata[1] = pageaddr >> 6;
	txdata[2] = (pageaddr & 0b00111111) << 2;

    SPI2_CS_LOW;
    rv = HAL_SPI_Transmit(&hspi2, txdata, 8, SPI2_TIMEOUT);
    if (rv == HAL_OK)
    	rv = HAL_SPI_Receive (&hspi2, buf,  512, SPI2_TIMEOUT);
    SPI2_CS_HIGH;
    return rv;
}

HAL_StatusTypeDef at45_read_page_ll(uint8_t *buf, uint16_t pageaddr)
{
    uint8_t txdata[8] = {0};
    txdata[0] = Main_Memory_Page_Read;
    txdata[1] = pageaddr >> 6;
    txdata[2] = (pageaddr & 0b00111111) << 2;

    SPI2_CS_LOW;
    /* commando + 4 dummy: HAL is prima voor 8 bytes (± 5 µs overhead) */
    if (HAL_SPI_Transmit(&hspi2, txdata, 8, SPI2_TIMEOUT) != HAL_OK) {
        SPI2_CS_HIGH;
        return HAL_ERROR;
    }
    /* data: strakke lus, alleen RXNE-flag en DR */
    for (uint16_t i = 0; i < 512; i++) {
        while (!(SPI2->SR & SPI_SR_TXE)) { }        // wacht tot TX verzet is
        *(uint8_t *) &SPI2->DR = 0xFF;              // dummy uitzenden → klok → chip schuift data in
        while (!(SPI2->SR & SPI_SR_RXNE)) { }       // nu komt de byte gegarandeerd
        buf[i] = (uint8_t) SPI2->DR;
    }
    SPI2_CS_HIGH;
    return HAL_OK;
}

/* ergens globaal */
static volatile uint8_t dma_rx_done;

/* de HAL-callback die de DMA-complete via SPI krijgt */
void HAL_SPI_RxCpltCallback_SPI2()
{
    dma_rx_done = 1;
}

HAL_StatusTypeDef at45_read_page_dma(uint8_t *buf, uint16_t pageaddr)
{
    uint8_t txdata[8] = {0};
    txdata[0] = Main_Memory_Page_Read;
    txdata[1] = pageaddr >> 6;
    txdata[2] = (pageaddr & 0b00111111) << 2;

    SPI2_CS_LOW;
    if (HAL_SPI_Transmit(&hspi2, txdata, 8, SPI2_TIMEOUT) != HAL_OK) {
        SPI2_CS_HIGH;
        return HAL_ERROR;
    }

    dma_rx_done = 0;
    if (HAL_SPI_Receive_DMA(&hspi2, buf, 512) != HAL_OK) {
        SPI2_CS_HIGH;
        return HAL_ERROR;
    }
    while (!dma_rx_done) { }          /* RAM-lees, geen peripheral-bus, geen flag-flits */
    while (SPI2->SR & SPI_SR_BSY) { } /* laatste byte uit de shifter */
    SPI2_CS_HIGH;
    return HAL_OK;
}

uint8_t at45_status(void)
{
	uint8_t rv;
	uint8_t opcode = Status_Register_Read;
	SPI2_CS_LOW;
	HAL_SPI_Transmit(&hspi2, &opcode, 1, SPI2_TIMEOUT);
	HAL_SPI_Receive (&hspi2, &rv,  1, SPI2_TIMEOUT);
	SPI2_CS_HIGH;
	return rv;
}

HAL_StatusTypeDef at45_ready_wait(void)
{
	uint32_t tries = 0;
	do {} while ((at45_status() & 0b10000000) == 0 && ++tries < 1000);
	if (tries < 1000)
		return HAL_OK;
	return HAL_ERROR;
}

HAL_StatusTypeDef at45_write_page(uint8_t *buf, uint16_t pageaddr)
{
	HAL_StatusTypeDef rv;
	uint8_t txdata[4] = {0};
	txdata[0] = Buffer_1_Write;

    SPI2_CS_LOW;
    rv = HAL_SPI_Transmit(&hspi2, txdata, 4, SPI2_TIMEOUT);
    if (rv == HAL_OK)
    	rv = HAL_SPI_Transmit (&hspi2, buf,  512, SPI2_TIMEOUT);
    SPI2_CS_HIGH;

    txdata[0] = Buffer_1_to_Main_Memory_Page_Program_with_Builtin_Erase;
	txdata[1] = pageaddr >> 6;
	txdata[2] = (pageaddr & 0b00111111) << 2;
	SPI2_CS_LOW;
	if (rv == HAL_OK)
		rv = HAL_SPI_Transmit(&hspi2, txdata, 4, SPI2_TIMEOUT);
    SPI2_CS_HIGH;

    if (rv == HAL_OK)
    	rv = at45_ready_wait();
    return rv;
}
