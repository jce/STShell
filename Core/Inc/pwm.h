/*
 * pwm.h
 *
 *  Created on: Sep 28, 2026
 *      Author: jeindhoven
 */

#ifndef INC_PWM_H_
#define INC_PWM_H_

enum pwm_mode_e { PWM_OFF, PWM_MANUAL, PWM_COMPASS };
#define ICOMPASS	47000			// Compass intensity

void pwm_set(uint16_t, uint16_t);	// Set pwm channel to value. [0-7] [0-47000]
void pwm_task(void);
void pwm_set_mode(uint8_t);
void tim3_irq_handler(void);
void tim4_irq_handler(void);

#endif /* INC_PWM_H_ */
