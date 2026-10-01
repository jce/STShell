/*
 * Author: Mistral
 * s_bench — snelheidstest AT45 driver (los van USB/MSC)
 *
 * Gebruik:
 *   bench read [n]   — leest n pagina's sequentieel (default 256)
 *   bench write [n]  — schrijft n pagina's sequentieel (default 16)
 *   bench            — beide
 *
 * Meet met DWT cyclusteller (f_Cortex) i.p.v. HAL_GetTick,
 * zodat korte runs niet op ms-resolutie afknappen.
 */

#include "main.h"
#include "AT45_bench.h"
#include <stdio.h>
#include "shell.h"
#include "AT45DB161D.h"
#include <string.h>

void bench_delay_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
//    DWT->LAR = 0xC5ACCE55;   // unlock, nodig op sommige STM32's
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t bench_ticks(void)
{
    return DWT->CYCCNT;
}

static void bench_report(const char *what, uint32_t cycles,
        uint32_t pages, uint32_t errors)
{
    // ms met 0.1 resolutie: cycles * 1000 / (SystemCoreClock/1000)
    uint32_t ms10 = (uint32_t) (((uint64_t) cycles * 10000ULL)
            / SystemCoreClock);
    uint32_t kbytes = pages * 512UL / 1024UL;
    char buf[96];
    if (ms10 == 0 || errors)
    {
        sprintf(buf, "%s: FAIL (%lu errors, %lu ms)\r\n",
                what, errors, ms10 / 10);
    }
    else
    {
        // kB/s = kbytes * 10000 / ms10
        uint32_t kbps = (uint32_t) (((uint64_t) kbytes * 10000ULL)
                / ms10);
        sprintf(buf, "%s: %lu pages (%lu kB) in %lu.%lu s = %lu kB/s\r\n",
                what, pages, kbytes, ms10 / 10000,
                (ms10 / 1000) % 10, kbps);
    }
    shell_tx_str(buf);
}

static void bench_read(uint32_t pages, uint8_t *buf)
{
    uint32_t errors = 0;
    bench_delay_init();
    uint32_t t0 = bench_ticks();
    for (uint32_t p = 0; p < pages; p++)
        if (at45_read_page(buf, p) != HAL_OK)
            errors++;
    uint32_t t1 = bench_ticks();
    // voorkomen dat de compiler de read weg-optimaliseert
    volatile uint8_t sink = buf[0] ^ buf[511];
    (void) sink;
    bench_report("read", t1 - t0, pages, errors);
}

static void bench_read_ll(uint32_t pages, uint8_t *buf)
{
    uint32_t errors = 0;
    bench_delay_init();
    uint32_t t0 = bench_ticks();
    for (uint32_t p = 0; p < pages; p++)
        if (at45_read_page_ll(buf, p) != HAL_OK)
            errors++;
    uint32_t t1 = bench_ticks();
    // voorkomen dat de compiler de read weg-optimaliseert
    volatile uint8_t sink = buf[0] ^ buf[511];
    (void) sink;
    bench_report("readll", t1 - t0, pages, errors);
}

static void bench_read_dma(uint32_t pages, uint8_t *buf)
{
    uint32_t errors = 0;
    bench_delay_init();
    uint32_t t0 = bench_ticks();
    for (uint32_t p = 0; p < pages; p++)
        if (at45_read_page_dma(buf, p) != HAL_OK)
            errors++;
    uint32_t t1 = bench_ticks();
    // voorkomen dat de compiler de read weg-optimaliseert
    volatile uint8_t sink = buf[0] ^ buf[511];
    (void) sink;
    bench_report("readdma", t1 - t0, pages, errors);
}

static void bench_write(uint32_t pages, uint8_t *buf)
{
    uint32_t errors = 0;
    // vul buf met iets herkenbaars, niet 0-en
    for (uint16_t i = 0; i < 512; i++)
        buf[i] = (uint8_t) i;

    bench_delay_init();
    uint32_t t0 = bench_ticks();
    for (uint32_t p = 0; p < pages; p++)
        if (at45_write_page(buf, 0x3d00 + p) != HAL_OK)  // testzone, niet p.0!
            errors++;
    uint32_t t1 = bench_ticks();
    bench_report("write", t1 - t0, pages, errors);
}

void s_bench(int argc, char **argv)
{
    static uint8_t buf[512];   // static: niet op de stack
    uint32_t n;

    if (argc >= 3 && strcmp(argv[1], "read") == 0
            && sscanf(argv[2], "%lu", &n) == 1)
        bench_read(n, buf);
    if (argc >= 3 && strcmp(argv[1], "readll") == 0
            && sscanf(argv[2], "%lu", &n) == 1)
        bench_read_ll(n, buf);
    if (argc >= 3 && strcmp(argv[1], "readdma") == 0
            && sscanf(argv[2], "%lu", &n) == 1)
        bench_read_dma(n, buf);
    else if (argc >= 3 && strcmp(argv[1], "write") == 0
            && sscanf(argv[2], "%lu", &n) == 1)
        bench_write(n, buf);
    else if (argc == 1)
    {
        shell_tx_str("read:\r\n");
        bench_read(256, buf);      // 128 kB
        shell_tx_str("read:\r\n");
        bench_read_ll(256, buf);      // 128 kB
        shell_tx_str("read:\r\n");
        bench_read_dma(256, buf);      // 128 kB
        shell_tx_str("write:\r\n");
        bench_write(16, buf);      // 16 pagina's is al genoeg voor erase-timing
    }
    else
        shell_tx_str("Usage: bench [read/readll/readdma/write] [pages]\r\n");
}
