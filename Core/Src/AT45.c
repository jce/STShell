/*
 * AT45DB161D.c
 *
 *  Created on: Sep 30, 2026
 *      Author: jeindhoven
 */

// Left the chip in factory default 528-bytes-per-page mode. Changing this is irreversible
// per flashchip, and requires changing the addressing scheme. Only the first 512 bytes are
// used now anyways.

#include <stdio.h>

#include <AT45.h>
#include "main.h"
#include "shell.h"
#include <string.h>

#define SPI2_CS_LOW  HAL_GPIO_WritePin(SPI2_CS_GPIO_Port, SPI2_CS_Pin, GPIO_PIN_RESET);
#define SPI2_CS_HIGH HAL_GPIO_WritePin(SPI2_CS_GPIO_Port, SPI2_CS_Pin, GPIO_PIN_SET);

//#define DBG(...) do {printf(__VA_ARGS__); printf("\r\n");} while (0)
#define DBG(...)

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
	DBG("Reading page %u", pageaddr);
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
    while (!dma_rx_done) { }
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
	DBG("Writing page %u", pageaddr);
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
    {
    	rv = at45_ready_wait();
    	if (rv == HAL_OK)
    		count_page_erase(pageaddr);
    }
    return rv;
}

// =======================================================================================
// Wear counter system
// =======================================================================================

uint32_t get_tally_count(uint8_t* record)
{
	// Count tally
	uint32_t tally = 0;
	for (int i = 0; i < TALLY_BYTES; i++)
		for (int j = 0; j < 8; j++)
		{
			if (record[3+i] >> j & 0b00000001)
				return tally;
			tally++;
		}
	return tally;
}

uint32_t get_total_count(uint8_t* record)
{
	uint32_t count = ((uint32_t)record[0] << 16) | ((uint32_t)record[1] << 8) | ((uint32_t)record[2]);
	if (count == 0x00FFFFFF)
		count = 0;
	else
		count = count + 1; 			// +1 for we start with 0 at 0xFFFFFF
	uint32_t tally = get_tally_count(record);
	return count+tally;
}

// Function assumes tally will fit in the tally area!
void increment_tally(uint8_t *record)
{
	// Count tally
	uint32_t tally = get_tally_count(record);

	// We are sure our tally bit can be appended to the record now.
	uint8_t tallybyte = 3+tally/8;
	uint8_t tallybit = tally%8;
	record[tallybyte] &= ~(1 << tallybit);
	DBG("Tally changed to %02X%02X%02X%02X%02X", record[3], record[4], record[5], record[6], record[7]);
}

void copy_tally_page_to_ram_buffer_1(uint32_t tally_page)
{
	uint8_t txdata[4];
	txdata[0] = Main_Memory_Page_to_Buffer_1_Transfer;
	txdata[1] = tally_page >> 6;
	txdata[2] = (tally_page & 0b00111111) << 2;
	txdata[3] = 0xff;		// Don't care bits, but we still care!
    SPI2_CS_LOW;
    HAL_SPI_Transmit(&hspi2, txdata, 4, SPI2_TIMEOUT);
    SPI2_CS_HIGH;
    at45_ready_wait();
}

void get_record_from_ram_buffer_1(uint32_t record_in_page, uint8_t *record)
{
	uint8_t txdata[5];
	uint32_t offset_in_page = record_in_page * RECORD_LENGTH;
	txdata[0] = Buffer_1_Read;
	txdata[1] = 0xff;
	txdata[2] = 0b11111100 | (offset_in_page >> 8);
	txdata[3] = offset_in_page & 0x000000ff;
	txdata[4] = 0xff;	// High speed read requires additional don't care byte.
    SPI2_CS_LOW;
    HAL_SPI_Transmit(&hspi2, txdata, 5, SPI2_TIMEOUT);
    HAL_SPI_Receive (&hspi2, record, 8, SPI2_TIMEOUT);
    SPI2_CS_HIGH;
	DBG("Received from ram buffer 1: %02X%02X%02X%02X%02X", record[3], record[4], record[5], record[6], record[7]);
}

void write_record_to_ram_buffer_1(uint32_t record_in_page, uint8_t *record)
{
	DBG("Writing to ram buffer 1: %02X%02X%02X%02X%02X", record[3], record[4], record[5], record[6], record[7]);
	uint8_t txdata[4];
	uint32_t offset_in_page = record_in_page * RECORD_LENGTH;
	txdata[0] = Buffer_1_Write;
	txdata[1] = 0xff;
	txdata[2] = 0b11111100 | (offset_in_page >> 8);
	txdata[3] = offset_in_page & 0x000000ff;
    SPI2_CS_LOW;
    HAL_SPI_Transmit(&hspi2, txdata, 4, SPI2_TIMEOUT);
    HAL_SPI_Transmit(&hspi2, record, 8, SPI2_TIMEOUT);
    SPI2_CS_HIGH;
}

void copy_ram_buffer_1_to_tally_page_noerase(uint32_t tally_page)
{
	uint8_t txdata[4];
	txdata[0] = Buffer_1_to_Main_Memory_Page_Program_without_Builtin_Erase;
	txdata[1] = tally_page >> 6;
	txdata[2] = (tally_page & 0b00111111) << 2;
	txdata[3] = 0xff;		// Don't care bits, but we still care!
    SPI2_CS_LOW;
    HAL_SPI_Transmit(&hspi2, txdata, 4, SPI2_TIMEOUT);
    SPI2_CS_HIGH;
    at45_ready_wait();
}

void copy_ram_buffer_1_to_tally_page(uint32_t tally_page)
{
	uint8_t txdata[4];
	txdata[0] = Buffer_1_to_Main_Memory_Page_Program_with_Builtin_Erase;
	txdata[1] = tally_page >> 6;
	txdata[2] = (tally_page & 0b00111111) << 2;
	txdata[3] = 0xff;		// Don't care bits, but we still care!
    SPI2_CS_LOW;
    HAL_SPI_Transmit(&hspi2, txdata, 4, SPI2_TIMEOUT);
    SPI2_CS_HIGH;
    at45_ready_wait();
}

void count_page_erase(uint32_t page)
{
	DBG("Counting erase for page %lu", page);

	uint32_t tally_page = page / RECORDS_PER_PAGE + WC_FIRST_PAGE;
	uint32_t record_in_page = page % RECORDS_PER_PAGE;
	uint8_t record[8];
	// Copy the tally page to ram buffer 1
	copy_tally_page_to_ram_buffer_1(tally_page);

	// Get the record from buffer 1
	get_record_from_ram_buffer_1(record_in_page, record);

	// Increment tally
	uint32_t tally = get_tally_count(record);
	DBG("Reading tally from page %lu record %lu: %lu", tally_page, record_in_page, tally);
	if (tally >= TALLY_NUM)
	{
		// we need to consolidate!
		consolidate_records_page(tally_page, record_in_page);
		// Note that consolidate also took into account our increment,
		// And writing of the tally page to flash. Basically, we're done.
	}
	else
	{
		// No need to consolidate.
		increment_tally(record);

		// Write the record back in buffer 1
		write_record_to_ram_buffer_1(record_in_page, record);

		// Write buffer 1 back into the tally page, without erase.
		copy_ram_buffer_1_to_tally_page_noerase(tally_page);
	}
}

void consolidate_records_page(uint32_t pagenr, uint32_t record_in_page)
{
	DBG("Consolidating page %lu, tallying record %lu", pagenr, record_in_page);
	// Get the whole page
	static uint8_t page[512];
	at45_read_page_dma(page, pagenr);

	// Consolidate the records individually
	for (int i = 0; i < RECORDS_PER_PAGE; i++)
	{
		uint8_t *rec = page + RECORD_LENGTH * i;
		uint32_t count = get_total_count(rec);
		count = count - 1;	 /* count==0 wraps by unsigned to 0xFFFFFFFF → 0xFFFFFF-field = sentinel: intended! */
		count += (i == record_in_page);		// Increment if this is the one record that tipped the bucket.
		rec[0] = (count & 0x00FF0000) >> 16;
		rec[1] = (count & 0x0000FF00) >> 8;
		rec[2] = (count & 0x000000FF) >> 0;
		rec[3] = rec[4] = rec[5] = rec[6] = rec[7] = 0xFF;
	}

	// Store the whole page
	at45_write_page(page, pagenr);
}

// ==================================================================================================
// Diagnostic functions

void wear_dump(shell_ctx_t *ctx)
{
	uint8_t page[512];
	for (int i = 0; i < PAGES_FOR_ALL_RECORDS; i++)
	{
		at45_read_page_dma(page, i+WC_FIRST_PAGE);
		for (int j = 0; j < RECORDS_PER_PAGE; j++)
		{
			if (j % 16 == 0)
				shell_printf(ctx, "%04u: ", i * RECORDS_PER_PAGE + j);
			uint32_t total = get_total_count(page + RECORD_LENGTH * j);
			shell_printf(ctx, "%8lu ", total);
			if (j % 16 == 15)
				shell_tx_str(ctx, "\r\n");
		}
	}
}

HAL_StatusTypeDef at45_dump_page(shell_ctx_t *ctx, uint32_t pagenr)
{
    uint8_t page[512];
    char buf[64];
    HAL_StatusTypeDef rv = at45_read_page_dma(page, pagenr);

    for (int row = 0; row < 512; row += 16)
    {
        shell_printf(ctx, "%03lX: ", pagenr * 512L + row);   /* byte-adres, of "%04lX: " paginanaam */
        for (int col = 0; col < 16; col++)
            shell_printf(ctx, "%02X ", page[row + col]);

        /* ASCII-kolom: leesbaarheid bij strings/structs */
        shell_tx_str(ctx, " |");
        for (int col = 0; col < 16; col++)
        {
            uint8_t c = page[row + col];
            buf[col] = (c >= 0x20 && c < 0x7F) ? c : '.';
        }
        buf[16] = 0;
        shell_tx_str(ctx, buf);
        shell_tx_str(ctx, "|\r\n");
    }
    return rv;
}

HAL_StatusTypeDef at45_dump_pages(shell_ctx_t *ctx, uint32_t first, uint32_t last)
{
    for (uint32_t p = first; p <= last; p++)
    {
        HAL_StatusTypeDef rv = at45_dump_page(ctx, p);
        if (rv != HAL_OK)
            return rv;
    }
    return HAL_OK;
}

HAL_StatusTypeDef at45_erase_page(uint32_t pagenr)
{
	DBG("Ersasing page %lu", pagenr);
    uint8_t txdata[4] = {0};
    txdata[0] = Page_Erase;
    txdata[1] = pagenr >> 6;
    txdata[2] = (pagenr & 0b00111111) << 2;
    txdata[3] = 0xff;

    SPI2_CS_LOW;
    HAL_StatusTypeDef rv = HAL_SPI_Transmit(&hspi2, txdata, 4, SPI2_TIMEOUT);
    SPI2_CS_HIGH;

    if (rv == HAL_OK)
        rv = at45_ready_wait();
    if (rv == HAL_OK)
        count_page_erase(pagenr);      // Count erases
    return rv;
}

HAL_StatusTypeDef at45_erase_pages(uint32_t first, uint32_t last)
{
    for (uint32_t p = first; p <= last; p++)
    {
        HAL_StatusTypeDef rv = at45_erase_page(p);
        if (rv != HAL_OK)
            return rv;
    }
    return HAL_OK;
}

void s_at(shell_ctx_t *ctx, int argc, char** argv)
{
	if ((argc >= 3) && strcmp(argv[1], "erase") == 0)
	{
		uint32_t from, to;
		sscanf(argv[2], "%lu", &from);
		if (argc >= 4)
			sscanf(argv[3], "%lu", &to);
		else
			to = from;
		at45_erase_pages(from, to);
		return;
	}

	if ((argc >= 3) && strcmp(argv[1], "dump") == 0)
	{
		uint32_t from, to;
		sscanf(argv[2], "%lu", &from);
		if (argc >= 4)
			sscanf(argv[3], "%lu", &to);
		else
			to = from;
		at45_dump_pages(ctx, from, to);
		return;
	}

	if ((argc >= 2) && strcmp(argv[1], "wear") == 0)
	{
		wear_dump(ctx);
		return;
	}
	shell_tx_str(ctx, "Commands: erase, dump, wear.\r\n");
}





