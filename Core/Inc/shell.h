/*
 * shell.h
 *
 *  Created on: Sep 10, 2026
 *      Author: jeindhoven
 */

#ifndef INC_SHELL_H_
#define INC_SHELL_H_

#define SHELL_PROMPT ">"
#define SHELL_VER "0.1.5"


void shell_rx(uint8_t);	// Receive a character. Whole processing hangs on this one function
void shell_register_tx(void (*)(uint8_t)); // Way to supply the shell with the tx function
extern void (*shell_tx)(char); // Transmit function for one character

void shell_tx_str(const char*);
void dispatch();


#define CBLEN 64				// Command Buffer Length
#define AVLEN 8			// The argument vector maximum length

#endif /* INC_SHELL_H_ */
