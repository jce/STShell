/*
 * pwm.h
 *
 *  Created on: Sep 28, 2026
 *      Author: jeindhoven
 */

#ifndef INC_PWM_H_
#define INC_PWM_H_

enum pwm_mode_e { PWM_OFF, PWM_MANUAL, PWM_COMPASS, PWM_RODO, PWM_USB };
#define IMAX		47000			// Max intensity
#define RODO_PERIOD	2.0f			// Rodo single circle time [s]
#define SCAN_DT		10				// [ms]
#define USB_TAU		0.1f			// [s]

void pwm_set(uint16_t, uint16_t);	// Set pwm channel to value. [0-7] [0-47000]
void pwm_task(void);
void pwm_set_mode(uint8_t);
void usb_event(void);
void tim3_irq_handler(void);
void tim4_irq_handler(void);

#endif /* INC_PWM_H_ */
