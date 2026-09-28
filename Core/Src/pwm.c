/*
 * pwm.c
 *
 *  Created on: Sep 28, 2026
 *      Author: jeindhoven
 */

#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include "stm32f303xc.h"
#include "main.h"


#include "pwm.h"

volatile uint16_t pwm[8] = {0, 0, 0, 0, 0, 0, 0, 0};

void pwm_set(uint16_t i, uint16_t v)
{
	if (i < 8)
		pwm[i] = v;
}

// Mistral wrote this. I follow what is happening and agree with it, but did not
// come up with it myself.
void init_ledpwm(void)
{
    /* 1. Klok op de timers (RCC, APB1) */
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN | RCC_APB1ENR_TIM4EN;
    (void)RCC->APB1ENR;                    /* dummy-read: klok-garantie (pipeline) */

    /* 2. Timers stil zetten en configureren */
    TIM3->CR1 = 0;                          /* teller uit, vanaf voren */
    TIM3->PSC = 0;                          /* geen prescaler */
    TIM3->ARR = 48000 - 1;                  /* 1 kHz bij 48 MHz timerklok */
    TIM3->CCR1 = TIM3->CCR2 = TIM3->CCR3 = TIM3->CCR4 = 0;
    TIM3->CNT = 0;
    TIM3->EGR = TIM_EGR_UG;                 /* update: PSC/ARR meteen laden */
    TIM3->SR  = 0;                          /* oude vlaggen weg */

    /* 3. Interrupten vrijgeven: update + 4 compares, per timer */
    TIM3->DIER = TIM_DIER_UIE | TIM_DIER_CC1IE | TIM_DIER_CC2IE
               | TIM_DIER_CC3IE | TIM_DIER_CC4IE;
    TIM4->DIER = TIM3->DIER;

    /* 4. Sync: TIM3 master (update-event als trigger), TIM4 slave */
//    TIM3->CR2 = (0x2 << TIM_CR2_MMS_Pos);   /* MMS = 010: update als trigger */
//    TIM4->SMCR = (0x2 << TIM_SMCR_SMS_Pos) /* SMS = 010: trigger reset-mode */
//               | (0x1 << TIM_SMCR_TS_Pos); /* TS = 001: ITR0/TIM3 doorverbonden */

    /* 5. NVIC: de twee IRQ's aan */
    NVIC_EnableIRQ(TIM3_IRQn);
    NVIC_EnableIRQ(TIM4_IRQn);

    /* 6. Nu pas aanzetten — master en slave gelijktijdig laten vertrekken */
    TIM4->CR1 = TIM_CR1_CEN;                /* slave eerst klaarzetten */
    TIM3->CR1 = TIM_CR1_CEN;                /* master start, trekt TIM4 mee */
}

void pwm_task(void)
{
	init_ledpwm();

	while(1)
	{

	    osDelay(1);
	}
}

// Preloading the pwms only at the rollover prevents skipping the turn-off-event
// in case of some updates.
void tim3_irq_handler(void)
{
	uint32_t sr = TIM3->SR;
	if (sr & TIM_SR_UIF)
	{
		uint16_t pwm0 = pwm[0];
		uint16_t pwm1 = pwm[1];
		uint16_t pwm2 = pwm[2];
		uint16_t pwm3 = pwm[3];
		if (pwm0)
			LD3_GPIO_Port->BSRR = LD3_Pin;
		if (pwm1)
			LD5_GPIO_Port->BSRR = LD5_Pin;
		if (pwm2)
			LD7_GPIO_Port->BSRR = LD7_Pin;
		if (pwm3)
			LD9_GPIO_Port->BSRR = LD9_Pin;
		TIM3->CCR1 = pwm0;
		TIM3->CCR2 = pwm1;
		TIM3->CCR3 = pwm2;
		TIM3->CCR4 = pwm3;
	}
	if (sr & TIM_SR_CC1IF)
		LD3_GPIO_Port->BSRR = (uint32_t)LD3_Pin << 16;
	if (sr & TIM_SR_CC2IF)
		LD5_GPIO_Port->BSRR = (uint32_t)LD5_Pin << 16;
	if (sr & TIM_SR_CC3IF)
		LD7_GPIO_Port->BSRR = (uint32_t)LD7_Pin << 16;
	if (sr & TIM_SR_CC4IF)
		LD9_GPIO_Port->BSRR = (uint32_t)LD9_Pin << 16;
	TIM3->SR = ~sr;
}

void tim4_irq_handler(void)
{
	uint32_t sr = TIM4->SR;
	if (sr & TIM_SR_UIF)
	{
		uint16_t pwm4 = pwm[4];
		uint16_t pwm5 = pwm[5];
		uint16_t pwm6 = pwm[6];
		uint16_t pwm7 = pwm[7];
		if (pwm4)
			LD10_GPIO_Port->BSRR = LD10_Pin;
		if (pwm5)
			LD8_GPIO_Port->BSRR = LD8_Pin;
		if (pwm6)
			LD6_GPIO_Port->BSRR = LD6_Pin;
		if (pwm7)
			LD4_GPIO_Port->BSRR = LD4_Pin;
		TIM4->CCR1 = pwm4;
		TIM4->CCR2 = pwm5;
		TIM4->CCR3 = pwm6;
		TIM4->CCR4 = pwm7;
	}
	if (sr & TIM_SR_CC1IF)
		LD10_GPIO_Port->BSRR = (uint32_t)LD10_Pin << 16;
	if (sr & TIM_SR_CC2IF)
		LD8_GPIO_Port->BSRR = (uint32_t)LD8_Pin << 16;
	if (sr & TIM_SR_CC3IF)
		LD6_GPIO_Port->BSRR = (uint32_t)LD6_Pin << 16;
	if (sr & TIM_SR_CC4IF)
		LD4_GPIO_Port->BSRR = (uint32_t)LD4_Pin << 16;
	TIM4->SR = ~sr;
}
