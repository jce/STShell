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
#include "math.h"

#include "LSM303AGR.h"
#include "nvstore.h"
#include "pwm.h"

volatile uint16_t pwm[8] = {0, 0, 0, 0, 0, 0, 0, 0};
volatile uint8_t pwm_mode = PWM_OFF;
volatile float usbsignal = 0.0f;
void pwm_set_mode(uint8_t mode)
{
	pwm_mode = mode;
	nvstore(NV_PWMMODE, 1, (void*) &pwm_mode);
}

void pwm_set(uint16_t i, uint16_t v)
{
	if (i < 8)
		pwm[i] = v;
}

// Mistral wrote this. I follow what is happening and agree with it, but did not
// come up with it myself.
void init_pwm(void)
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

void deinit_pwm(void)
{
    TIM3->DIER = 0;
    TIM4->DIER = 0;
    NVIC_DisableIRQ(TIM3_IRQn);
    NVIC_DisableIRQ(TIM4_IRQn);
    TIM3->CR1 = 0;
    TIM4->CR1 = 0;
    //TIM3->CCER = 0;
    //TIM4->CCER = 0;

    LD3_GPIO_Port->BSRR = (uint32_t)LD3_Pin << 16;
    LD5_GPIO_Port->BSRR = (uint32_t)LD5_Pin << 16;
    LD7_GPIO_Port->BSRR = (uint32_t)LD7_Pin << 16;
    LD9_GPIO_Port->BSRR = (uint32_t)LD9_Pin << 16;
    LD10_GPIO_Port->BSRR = (uint32_t)LD10_Pin << 16;
    LD8_GPIO_Port->BSRR = (uint32_t)LD8_Pin << 16;
    LD6_GPIO_Port->BSRR = (uint32_t)LD6_Pin << 16;
    LD4_GPIO_Port->BSRR = (uint32_t)LD4_Pin << 16;
}

void pwm_task(void)
{
	void* p = nvfind(NV_PWMMODE);
	if (p)
		pwm_mode = * (uint8_t*) p;
	uint8_t pwm_mode_prev = PWM_OFF;

	while(1)
	{
		if (pwm_mode != pwm_mode_prev && pwm_mode_prev == PWM_OFF)
			init_pwm();
		if (pwm_mode != pwm_mode_prev && pwm_mode == PWM_OFF)
			deinit_pwm();

		if (pwm_mode == PWM_COMPASS)
		{
			pwm[0] = pwm[1] = pwm[2] = pwm[3] = pwm[4] = pwm[5] = pwm[6] = pwm[7] = 0;
			float heading = atan2f(-lsm303agr.mag.x, lsm303agr.mag.y) / ((float)M_PI * 2);
			if (heading < 0)
				heading += 1.0f;					// Heading [0..1] for one rotation
			float leftf = heading * 8;
			int left = (int) leftf;
			if (left > 7)							// prevents left == 8
				left = 0;
			float ipart = leftf - left;
			pwm[left] = 			(uint16_t) ((1-ipart) * IMAX);
			pwm[(left + 1) % 8] =	(uint16_t) (ipart * IMAX);
		}

		if (pwm_mode == PWM_RODO)
		{
			pwm[0] = pwm[1] = pwm[2] = pwm[3] = pwm[4] = pwm[5] = pwm[6] = pwm[7] = 0;

			float now = (float) osKernelGetTickCount() / osKernelGetTickFreq();
			float phase = fmodf(now / RODO_PERIOD, 1.0f);

			float leftf = phase * 8;
			int left = (int) leftf;
			if (left > 7)							// prevents left == 8
				left = 0;
			float ipart = leftf - left;
			pwm[left] = 			(uint16_t) ((1-ipart) * IMAX);
			pwm[(left + 1) % 8] =	(uint16_t) (ipart * IMAX);
		}

		if (pwm_mode == PWM_USB)
		{
			if (pwm_mode_prev != PWM_USB)
				usbsignal = 0.0f;
			//float dt = (float)SCAN_DT / 1000.0f;
			pwm[0] = pwm[1] = pwm[2] = pwm[3] = pwm[4] = pwm[5] = pwm[6] = pwm[7] = usbsignal * IMAX;
			usbsignal *= expf(-SCAN_DT / (1000.0f * USB_TAU));
		}

		pwm_mode_prev = pwm_mode;
	    osDelay(SCAN_DT);
	}
}

void usb_event(void)
{
	usbsignal = 1.0f;
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
