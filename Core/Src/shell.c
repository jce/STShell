#include "stm32f3xx_hal.h"	// Fixes uint8_t being unknown.
#include "shell.h"
#include <string.h>

void (*shell_tx)(uint8_t); // Transmit function for one character
void shell_register_tx(void (*_shell_tx)(uint8_t))
{
	shell_tx = _shell_tx;
}

#define CBLEN 16				// Command Buffer Length
char commandbuf[CBLEN+1] = {0};	// Command buffer
int cbpos = 0;					// Pointer position in command buffer.

// Send a complete string via shell_tx
void shell_tx_str(const char *p)
{
	while (*p)
	{
		shell_tx(*p);
		p++;
	}
}

void shell_process_cbuf()
{
	char *cbufp = commandbuf;
	while (*cbufp)
	{
		shell_tx(*cbufp);
		cbufp++;
	}
}

void move_line_right(char c)	// For inserting characters at cbpos and moving the remainder of the line right.
{
	if (cbpos >= CBLEN)							// Refuse new characters if the string is full.
		return;

	char *tail = commandbuf + cbpos;			// The tail is the section that will be moved
	int tail_len = strlen(tail);				// It has a length

	for (int i = tail_len; i>=0; i--)			// Move the tail to make space.
		if (i+1+cbpos < CBLEN)					// Drop section of the tail that is too long.
			tail[i+1] = tail[i];

	commandbuf[cbpos] = c;						// Place our character in the newly available slot
	cbpos++;									// Increment cursor (potentially hitting CBLEN)

	int keep_tail = strlen(commandbuf+cbpos);	// The tail length may have changed
	shell_tx_str(commandbuf+cbpos-1);			// Print our new character c plus the tail

	for (int i = keep_tail; i > 0; i--)			// Snap back the tail length
		shell_tx_str("\x1B[D");					// By sending back arrows.
}

void move_line_left()	// For deleting characters at cbpos, and moving the remainder of the line left.
{
	if (0 == cbpos)										// Cursor already leftmost? Done.
		return;

	cbpos--;											// Cursor to left.
	shell_tx_str("\x1B[D");								// Displayed string: cursor to left.
	char *line_right_of_cursor = commandbuf + cbpos;	// We are going to overwrite what is at the cursor.
	int to_move = strlen(line_right_of_cursor);			// We also want to copy the first null character.
	for (int i = 0; i < to_move; i++)
	{
		line_right_of_cursor[i] = line_right_of_cursor[i+1];
		if (line_right_of_cursor[i])
			shell_tx(line_right_of_cursor[i]);
		else
			shell_tx(' ');
	}
	to_move = strlen(line_right_of_cursor);				// String is probably changed.;
	for (int i = to_move+1; i; i--)
	{
		shell_tx_str("\x1B[D");
	}
}

void reinstate_line()	// Doskey functionality for one line only.
{
	int buf_len = strlen(commandbuf);
	if (cbpos == 0 && buf_len > 0)
	{
		for (int i = 0; i < buf_len; i++)
			shell_tx(commandbuf[i]);
		cbpos = buf_len;
	}
}

void clear_line()	// Inverse of the doskey, clear the line.
{
	for (int i = 0; i < cbpos; i++)
	{
		shell_tx_str("\x1B[D");
		shell_tx(' ');
		shell_tx_str("\x1B[D");
	}
	//memset(commandbuf, 0, sizeof(commandbuf));
	cbpos = 0;
}

typedef enum { RX_NORMAL, RX_ESC, RX_ESC_BRACKET } rx_state_t;
static rx_state_t rx_state = RX_NORMAL;
void shell_rx(uint8_t c)
{
	if (RX_ESC == rx_state)
	{
		if ('[' == c)
			rx_state = RX_ESC_BRACKET;
		else
			rx_state = RX_NORMAL;
		return;
	}

	if (RX_ESC_BRACKET == rx_state)
	{
		if ('D' == c)		{ /* left arrow */ if (cbpos > 0) 					{ cbpos--; shell_tx_str("\x1B[D");} }
		else if ('C' == c)	{ /* right arrow */if (cbpos < strlen(commandbuf)) 	{ cbpos++; shell_tx_str("\x1B[C");} }
		else if ('A' == c)	{ /* up arrow */ 	reinstate_line(); 	}
		else if ('B' == c)	{ /* down arrow */ 	clear_line();		}
		rx_state = RX_NORMAL;
		return;
	}

	// From here, rx_state is always NORMAL
	if (0x1B == c)
	{
		rx_state = RX_ESC;
		return;
	}

	if (0 == c)// Ignore null
		return;

	if (0x7f == c || 0x08 == c)
	{
		move_line_left();
		return;
	}

	if ('\r' == c)	// In case a return is typed
	{
		shell_tx('\r');
		shell_tx('\n');
		if (cbpos != 0)
			shell_process_cbuf();
		cbpos = 0;
		shell_tx_str("\r\n" SHELL_PROMPT);
		return;
	}

	// In case a non return is typed
	if (cbpos == 0)
		memset(commandbuf, 0, sizeof(commandbuf));
	move_line_right(c);
}
