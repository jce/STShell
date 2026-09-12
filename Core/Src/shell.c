#include "stm32f3xx_hal.h"	// Fixes uint8_t being unknown.
#include "shell.h"
#include <string.h>	// strlen, strcmp
#include <stdio.h>	// sprintf
#include "stdbool.h"
//======================================================================
// Line editor logic

void (*shell_tx)(uint8_t); // Transmit function for one character
void shell_register_tx(void (*_shell_tx)(uint8_t))
{
	shell_tx = _shell_tx;
}

char commandbuf[CBLEN+1] = {0};	// Command buffer
int cbpos = 0;					// Pointer position in command buffer.
bool new_command = true;		// True if the command to be typed is new.

// Send a complete string via shell_tx
void shell_tx_str(const char *p)
{
	while (*p)
	{
		shell_tx(*p);
		p++;
	}
}

void dispatch();

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
	new_command = false;
}

void clear_line()	// Inverse of the doskey, clear the line.
{
	int buf_len = strlen(commandbuf);
	for (int i = 0; i < buf_len-cbpos; i++)	// There can be characters right of cursor position
		shell_tx_str("\x1B[C");				// Move to the end.
	for (int i = 0; i < buf_len; i++)		// Then clear character by
	{										// character to the left.
		shell_tx_str("\x1B[D");
		shell_tx(' ');
		shell_tx_str("\x1B[D");
	}
	cbpos = 0;
	new_command = true;
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
			dispatch();
		cbpos = 0;
		new_command = true;
		shell_tx_str(SHELL_PROMPT);
		return;
	}

	// In case a non return is typed
	if (new_command)
		memset(commandbuf, 0, sizeof(commandbuf));
	move_line_right(c);
	new_command = false;
}

//====================================================================
// Dispatcher logic

// Xmacro for commands. Members: command, function, helptext
#define COMMANDS \
CMD(help, 		s_help, 	"Shows help") \
CMD(version,	s_version, 	"Shows versions") \
CMD(test,		s_test, 	"Test arguments") \
CMD(clear,		s_clear, 	"Clear screen")

// Types
typedef void (*cmd_func_t)(int argc, char **argv);
typedef struct
{
	const char *cmd;
	cmd_func_t fnc;
} cmd_t;

// Function prototypes
#define CMD(NAME, FNC, HELP) void FNC (int, char **);
COMMANDS
#undef CMD

// Function table
static const cmd_t commands[] =
{
	#define CMD(NAME, FNC, HELP) {#NAME, FNC},
	COMMANDS
	#undef CMD
	{NULL, NULL}
};

void detokenize(int cblen)
{
	char *p = commandbuf;
	for (int j = cblen; j; j--)
	{
		if (*p == 0)
			*p = ' ';
		p++;
	}
}

// Dispatcher. Reads from global state commandbuf, cbpos.
void dispatch(void)
{
	int cblen = strlen(commandbuf);
	char *argv[AVLEN];
	int argc = 0;

	char *p = commandbuf;
	while (*p && argc < AVLEN)
	{
		while (*p == ' ')
			p++;
		if (!*p)
			break;
		argv[argc] = p;
		argc++;
		while (*p && *p != ' ')
			p++;
		if (*p)
		{
			*p = 0;
			p++;
		}
	}

	if (argc == 0)
		return;

	for (int i = 0; commands[i].cmd; i++)
		if (strcmp(argv[0], commands[i].cmd) == 0)
		{
			commands[i].fnc(argc, argv);
			detokenize(cblen);
			return;
		}

	shell_tx_str("Unknown command\r\n");
	detokenize(cblen);
}



//===========================================================================
// Commands

void s_help(int argc, char **argv)
{
	shell_tx_str("STShell " SHELL_VER "\r\n");
#define CMD(NAME, FNC, HELP) shell_tx_str(#NAME " - " HELP "\r\n");
	COMMANDS
#undef CMD
}

void s_version(int argc, char ** argv)
{
	char buf[80];
	shell_tx_str("STShell " SHELL_VER "\r\n");

	// Mistral helped me out with this.

	#define FLASH_SIZE_REG    (*(uint16_t *)0x1FFFF7CC) // Flash size in KB
	#define UID_REG           ((uint32_t *)0x1FFFF7AC)	// Unique device ID (96 bits = 3 words)
	#define DBGMCU_IDCODE     (*(uint32_t *)0xE0042000) // Device ID (tells you which chip in the family)
	// Bits [15:0] = device ID, bits [31:16] = revision
    uint32_t idcode = DBGMCU_IDCODE;
    sprintf(buf, "Device ID: 0x%03X\r\n", (unsigned int) (idcode & 0xFFF));
    shell_tx_str(buf);
    sprintf(buf, "Revision: 0x%04X\r\n", (unsigned int) ((idcode >> 16) & 0xFFFF));
    shell_tx_str(buf);
    sprintf(buf, "Flash: %u KB\r\n", FLASH_SIZE_REG);
    shell_tx_str(buf);
    sprintf(buf, "UID: %08lX%08lX%08lX\r\n", UID_REG[0], UID_REG[1], UID_REG[2]);
    shell_tx_str(buf);
    // Bits [31:24] = Implementer (0x41 = ARM)
    // Bits [23:20] = Variant (major revision)
    // Bits [19:16] = Architecture (0xF = Cortex-M4)
    // Bits [15:4]  = Part number (0xC24 = Cortex-M4)
    // Bits [3:0]   = Revision (minor revision rXpY)
    sprintf(buf, "Core: ARM r%lup%lu\r\n",
        (SCB->CPUID >> 20) & 0xF,   // Variant (major)
        SCB->CPUID & 0xF);           // Revision (minor)
    shell_tx_str(buf);

    shell_tx_str("Build: " __DATE__ " " __TIME__ "\r\n");
    shell_tx_str("Compiler version: " __VERSION__ "\r\n");
#ifdef __GNUC__
    sprintf(buf, "Compiler: GCC %d.%d.%d\r\n", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#elif defined(__ICCARM__)
    sprintf(buf, "Compiler: IAR\r\n");
#elif defined(__CC_ARM)
    sprintf(buf, "Compiler: ARMCC\r\n");
#endif
    shell_tx_str(buf);

    uint32_t v = HAL_GetHalVersion();
    sprintf(buf, "HAL: %lu.%lu.%lu\r\n",
        (v >> 24) & 0xFF,
        (v >> 16) & 0xFF,
        (v >> 8)  & 0xFF);
    shell_tx_str(buf);

    sprintf(buf, "CMSIS: %u.%u\r\n",
    		__CM_CMSIS_VERSION_MAIN,
			__CM_CMSIS_VERSION_SUB);
    shell_tx_str(buf);
    sprintf(buf, "CMSIS Device: %u.%u.%u\r\n",
    		__STM32F3_CMSIS_VERSION_MAIN,  // MAIN
			__STM32F3_CMSIS_VERSION_SUB1,  // SUB1
			__STM32F3_CMSIS_VERSION_SUB2); // SUB2
    shell_tx_str(buf);
}

void s_test(int argc, char **argv)
{
	for (; argc; argc--)
	{
		shell_tx_str(*argv);
		shell_tx_str("\r\n");
		argv++;
	}
}

void s_clear(int argc, char **argv)
{
	shell_tx_str("\033[0;0H" "\033[0;0H");
}


































