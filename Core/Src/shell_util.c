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

// Written by Mistral
void s_rd_hex_line(uint32_t addr, uint8_t *data, uint32_t len)
{
    // Address
    shell_tx_hex(addr);

    shell_tx_str(": ");

    // Hex bytes
    for (uint32_t i = 0; i < len; i++)
    {
    	if ((i & 0x7) == 0)	// Extra spatie na 8 bytes voor leesbaarheid;
    		shell_tx(' ');
        uint8_t hi = (data[i] >> 4) & 0xF;
        uint8_t lo = data[i] & 0xF;
        shell_tx(hi < 10 ? '0' + hi : 'A' + hi - 10);
        shell_tx(lo < 10 ? '0' + lo : 'A' + lo - 10);
        shell_tx(' ');
    }

    // Padding als len < 16
    for (uint32_t i = len; i < 16; i++)
    {
        shell_tx_str("   ");
        if (i == 7)
            shell_tx(' ');
    }

    // ASCII
    shell_tx_str(" |");
    for (uint32_t i = 0; i < len; i++)
    {
        shell_tx(data[i] >= 0x20 && data[i] < 0x7F ? data[i] : '.');
    }
    // Padding ASCII
    for (uint32_t i = len; i < 16; i++)
        shell_tx(' ');
    shell_tx_str("|\r\n");
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

int32_t get_Vrefint()
{
	ADC_ChannelConfTypeDef channelconfig = {0};
	channelconfig.Channel = ADC_CHANNEL_VREFINT;
	channelconfig.Rank = ADC_REGULAR_RANK_1;
	channelconfig.SamplingTime = ADC_SAMPLETIME_601CYCLES_5;
	HAL_ADC_ConfigChannel(&hadc1, &channelconfig);

	HAL_ADC_Start(&hadc1);
	HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
	return HAL_ADC_GetValue(&hadc1);
}

#define REFINT_VDDA 3300		// [mV]
#define VREFINT_CAL (* (uint16_t*)0x1FFFF7BA) // ADC readout at 3300 mV, 30 degC

// Gets vdda ( from VRef )
int32_t get_vdda()
{
	// VREFINT_CAL / Vrefint_actual vangt de verschaling voor het verschil met de voedingsspanning.
	return REFINT_VDDA * VREFINT_CAL / get_Vrefint();
}

#define TS_V25		1.395f
#define TS_SLOPE_C	(-0.0043)

// Gets temperature in 0.1 degC
int32_t get_temp()
{
	ADC_ChannelConfTypeDef channelconfig = {0};
	channelconfig.Channel = ADC_CHANNEL_TEMPSENSOR;
	channelconfig.Rank = ADC_REGULAR_RANK_1;
	channelconfig.SamplingTime = ADC_SAMPLETIME_601CYCLES_5;
	HAL_ADC_ConfigChannel(&hadc1, &channelconfig);

	HAL_ADC_Start(&hadc1);
	HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
	uint32_t adc_val = HAL_ADC_GetValue(&hadc1);

	// VSense can have quite a range, from the datasheet. Made
	// a hardcoded calibration here. Its not correct, i know,
	// having magic numbers, and not backed by any source other
	// than experimentation.
	float vSense = (float) adc_val * get_vdda() / 4095 / 1000;
	float temp = (vSense - TS_V25) / TS_SLOPE_C + 25;
	return temp*10;

	// This does not work. Gives about 10 degC too low.
//	#define TS_CAL1  (*(uint16_t *)0x1FFFF7B8)  // 30°C
//	#define TS_CAL2  (*(uint16_t *)0x1FFFF7C2)  // 110°C
//	#define TS_CAL1_TEMP  30
//	#define TS_CAL2_TEMP  110
//	adc_val = adc_val * VREFINT_CAL / get_Vrefint();
//	int32_t temp_x10 = (TS_CAL2_TEMP - TS_CAL1_TEMP) * 10 * ((int32_t) adc_val - TS_CAL1) /
//			(TS_CAL2 - TS_CAL1) + TS_CAL1_TEMP * 10;
//	return temp_x10;
}

// Gets Vbat in mV
int32_t get_vbat()
{
	ADC_ChannelConfTypeDef channelconfig = {0};
	channelconfig.Channel = ADC_CHANNEL_VBAT;
	channelconfig.Rank = ADC_REGULAR_RANK_1;
	channelconfig.SamplingTime = ADC_SAMPLETIME_601CYCLES_5;
	HAL_ADC_ConfigChannel(&hadc1, &channelconfig);

	HAL_ADC_Start(&hadc1);
	HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
	uint32_t adc_val = HAL_ADC_GetValue(&hadc1);

	return adc_val * get_vdda() * 2 / 4095;
}

int32_t get_adc(ADC_HandleTypeDef* adc, uint32_t channel)
{
	if (channel > 18)
		return 0;

	if (channel < 1)
		return 0;

	ADC_ChannelConfTypeDef channelconfig = {0};
	channelconfig.Channel = ADC_CHANNEL_1 + (channel-1);
	channelconfig.Rank = ADC_REGULAR_RANK_1;
	channelconfig.SamplingTime = ADC_SAMPLETIME_601CYCLES_5;
	HAL_ADC_ConfigChannel(adc, &channelconfig);

	HAL_ADC_Start(adc);
	HAL_ADC_PollForConversion(adc, HAL_MAX_DELAY);
	uint32_t adc_val = HAL_ADC_GetValue(adc);

	return adc_val ;
}



















