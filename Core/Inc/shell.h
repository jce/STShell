/*
 * shell.h
 *
 *  Created on: Sep 10, 2026
 *      Author: jeindhoven
 */

#ifndef INC_SHELL_H_
#define INC_SHELL_H_

#include "stdbool.h"
#include <stdint.h>
#include <stddef.h>

#define SHELL_PROMPT ">"
#define SHELL_VER "0.5.6"


#define CBLEN 128			// Command Buffer Length
#define AVLEN 12			// The argument vector maximum length
typedef enum { RX_NORMAL, RX_ESC, RX_ESC_BRACKET, RX_ESC_BRACKET_3 } rx_state_t;

typedef struct shell_ctx_s
{
	int (*tx)(void *ctx, const char *buf, size_t len);
	int (*rx)(void *ctx, char *buf, size_t len);
	int (*flush)(void *ctx);
	char commandbuf[CBLEN+1];	// Command buffer
	int cbpos;					// Pointer position in command buffer.
	bool new_command;			// True if the command to be typed is new.
	rx_state_t rx_state;
	void *rx_ctx, *tx_ctx, *flush_ctx;
} shell_ctx_t;

#define NUM_SHELL	2
extern shell_ctx_t shell_ctx[NUM_SHELL];

void shell_task();			// For the RTOS
void shell_rx(shell_ctx_t*, uint8_t);	// Receive a character. Whole processing hangs on this one function
//void shell_register_tx(void (*)(uint8_t)); // Way to supply the shell with the tx function
//extern void (*shell_tx)(char); // Transmit function for one character

int shell_tx(shell_ctx_t *ctx, char);
int shell_tx_str(shell_ctx_t*, const char*);
#define SHELL_PRINTF_BUFLEN	128
int shell_printf(shell_ctx_t*, const char*, ...);
void dispatch(shell_ctx_t*);

int shell_tx_uart1(void*, const char*, size_t);
int shell_rx_uart1(void*, char*, size_t);

#endif /* INC_SHELL_H_ */
