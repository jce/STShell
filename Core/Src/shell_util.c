/*
 * shell_util.c
 *
 *  Created on: Sep 13, 2026
 *      Author: jeindhoven
 */
#include "main.h"
#include "stm32f3xx_it.h"
#include "shell.h"
#include "shell_util.h"

void shell_tx_hex(uint32_t val)
{
    shell_tx_str("0x");
    for (int i = 7; i >= 0; i--)
    {
        uint8_t nib = (val >> (i * 4)) & 0xF;
        shell_tx(nib < 10 ? '0' + nib : 'A' + nib - 10);
    }
}

// Mistral coded this for me! I am not knowledgeable on inline ASM!
//void HardFault_Handler(void)
//{
//    // Uitklokken bij unaligned access
//    // pui32StackFrame bevat de registers op moment van crash:
//    uint32_t *frame;
//    __asm volatile (
//        "tst lr, #4            \n"
//        "ite eq                \n"
//        "mrseq r0, msp         \n"
//        "mrsne r0, psp         \n"
//        : "=r" (frame)
//    );
//
//    uint32_t r0  = frame[0];
//    uint32_t r1  = frame[1];
//    uint32_t r2  = frame[2];
//    uint32_t r3  = frame[3];
//    uint32_t r12 = frame[4];
//    uint32_t lr  = frame[5];
//    uint32_t pc  = frame[6];  // ← pc bij crash
//    uint32_t psr = frame[7];
//
//    // Bfsregreg info via de shell
//    shell_tx_str("\r\n*** HARDFAULT ***\r\n");
//    shell_tx_str("R0:  "); shell_tx_hex(r0);  shell_tx_str("\r\n");
//    shell_tx_str("R1:  "); shell_tx_hex(r1);  shell_tx_str("\r\n");
//    shell_tx_str("R2:  "); shell_tx_hex(r2);  shell_tx_str("\r\n");
//    shell_tx_str("R3:  "); shell_tx_hex(r3);  shell_tx_str("\r\n");
//    shell_tx_str("R12: "); shell_tx_hex(r12); shell_tx_str("\r\n");
//    shell_tx_str("LR:  "); shell_tx_hex(lr);  shell_tx_str("\r\n");
//    shell_tx_str("PC:  "); shell_tx_hex(pc);  shell_tx_str("\r\n");
//    shell_tx_str("PSR: "); shell_tx_hex(psr); shell_tx_str("\r\n");
//
//    // HardFault status
//    uint32_t cfsr = SCB->CFSR;
//    shell_tx_str("CFSR: "); shell_tx_hex(cfsr); shell_tx_str("\r\n");
//
//    while (1);  // blijf hangen
//}


//__attribute__((naked)) void HardFault_Handler(void)
//{
//    __asm volatile (
//        "tst lr, #4          \n"
//        "ite eq              \n"
//        "mrseq r0, msp       \n"
//        "mrsne r0, psp       \n"
//        "b HardFault_C_Handler \n"
//    );
//}
//
//void HardFault_C_Handler(uint32_t *frame)
//{
//    // frame is nu gegarandeerd goed — MSP of PSP
//    uint32_t r0  = frame[0];
//    uint32_t r1  = frame[1];
//    uint32_t r2  = frame[2];
//    uint32_t r3  = frame[3];
//    uint32_t r12 = frame[4];
//    uint32_t lr  = frame[5];
//    uint32_t pc  = frame[6];
//    uint32_t psr = frame[7];
//
//    // ... print met blocking TX ...
//    //    // Bfsregreg info via de shell
//        shell_tx_str("\r\n*** HARDFAULT ***\r\n");
//        shell_tx_str("R0:  "); shell_tx_hex(r0);  shell_tx_str("\r\n");
//        shell_tx_str("R1:  "); shell_tx_hex(r1);  shell_tx_str("\r\n");
//        shell_tx_str("R2:  "); shell_tx_hex(r2);  shell_tx_str("\r\n");
//        shell_tx_str("R3:  "); shell_tx_hex(r3);  shell_tx_str("\r\n");
//        shell_tx_str("R12: "); shell_tx_hex(r12); shell_tx_str("\r\n");
//        shell_tx_str("LR:  "); shell_tx_hex(lr);  shell_tx_str("\r\n");
//        shell_tx_str("PC:  "); shell_tx_hex(pc);  shell_tx_str("\r\n");
//        shell_tx_str("PSR: "); shell_tx_hex(psr); shell_tx_str("\r\n");
//
//        // HardFault status
//        uint32_t cfsr = SCB->CFSR;
//    //    shell_tx_str("CFSR: "); shell_tx_hex(cfsr); shell_tx_str("\r\n");
//    while (1);
//}


void s_rd_hex_line(uint32_t addr, uint8_t *data, uint32_t len)
{
    // Address
    shell_tx_str("0x");
    for (int i = 7; i >= 0; i--)
    {
        uint8_t nib = (addr >> (i * 4)) & 0xF;
        shell_tx(nib < 10 ? '0' + nib : 'A' + nib - 10);
    }

    shell_tx_str(": ");

    // Hex bytes
    for (uint32_t i = 0; i < len; i++)
    {
        uint8_t hi = (data[i] >> 4) & 0xF;
        uint8_t lo = data[i] & 0xF;
        shell_tx(hi < 10 ? '0' + hi : 'A' + hi - 10);
        shell_tx(lo < 10 ? '0' + lo : 'A' + lo - 10);
        shell_tx(' ');
    }

    // Padding als len < 8
    for (uint32_t i = len; i < 8; i++)
        shell_tx_str("   ");

    // ASCII
    shell_tx(' ');
    for (uint32_t i = 0; i < len; i++)
    {
        shell_tx(data[i] >= 0x20 && data[i] < 0x7F ? data[i] : '.');
    }
    shell_tx_str("\r\n");
}

uint32_t s_atoi_hex(const char *s)
{
    uint32_t val = 0;

    // Skip "0x" als aanwezig
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        s += 2;

    while (*s)
    {
        char c = *s;
        if (c >= '0' && c <= '9')
            val = (val << 4) | (c - '0');
        else if (c >= 'a' && c <= 'f')
            val = (val << 4) | (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            val = (val << 4) | (c - 'A' + 10);
        else
            break;  // stop bij ongeldig teken
        s++;
    }
    return val;
}
