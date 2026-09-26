#include <stdio.h>	// sprintf
#include "stdbool.h"
#include "stm32f3xx_hal.h"	// Fixes uint8_t being unknown.
#include <string.h>	// strlen, strcmp

#include "flash_counter.h"
#include "LSM303AGR.h"
#include "main.h"
#include "shell.h"
#include "shell_util.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
//======================================================================
// Line editor logic


void shell_tx(char c)
{
	xQueueSend(STShell_TXHandle, &c, portMAX_DELAY);	// Fills the TX queue.
	USART1->CR1 |= USART_CR1_TXEIE;						// Starts the TX interrupt chain to transmit the TX queue.
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

void move_line_right(char c)	// For inserting characters at cbpos and moving the remainder of the line right.
{
	if (cbpos >= CBLEN)							// Refuse new characters if the string is full.
		return;

	char *tail = commandbuf + cbpos;			// The tail is the section that will be moved
	int tail_len = strlen((char*) tail);				// It has a length

	for (int i = tail_len; i>=0; i--)			// Move the tail to make space.
		if (i+1+cbpos < CBLEN)					// Drop section of the tail that is too long.
			tail[i+1] = tail[i];

	commandbuf[cbpos] = c;						// Place our character in the newly available slot
	cbpos++;									// Increment cursor (potentially hitting CBLEN)

	int keep_tail = strlen((char*) commandbuf+cbpos);	// The tail length may have changed
	shell_tx_str(commandbuf+cbpos-1);			// Print our new character c plus the tail

	for (int i = keep_tail; i > 0; i--)			// Snap back the tail length
		shell_tx_str("\x1B[D");					// By sending back arrows.
}

void delete()
{
	if (cbpos == strlen(commandbuf))
		return;

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

void backspace()	// For deleting characters at cbpos, and moving the remainder of the line left.
{
	if (0 == cbpos)										// Cursor already leftmost? Done.
		return;

	cbpos--;											// Cursor to left.
	shell_tx_str("\x1B[D");								// Displayed string: cursor to left.
	delete();
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

typedef enum { RX_NORMAL, RX_ESC, RX_ESC_BRACKET, RX_ESC_BRACKET_3 } rx_state_t;
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
		 if		('3' == c)	{ rx_state = RX_ESC_BRACKET_3;			}
		return;
	}

	if (RX_ESC_BRACKET_3 == rx_state)
	{
		if ('~' == c)
			delete();
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
		backspace();
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

void shell_task()
{
	char c;
	while (1)
		if (xQueueReceive(STShell_RXHandle, &c, portMAX_DELAY) == pdTRUE)
			shell_rx(c);
}

//====================================================================
// Dispatcher logic

// Xmacro for commands. Members: command, function, helptext
#define COMMANDS \
CMD(help, 		s_help, 	"Shows help.") \
CMD(?,			s_help,		"Help alias.") \
CMD(version,	s_version, 	"Shows versions.") \
CMD(test,		s_test, 	"Test arguments.") \
CMD(clear,		s_clear, 	"Clear screen.") \
CMD(live,		s_live,		"Live view of peripherhals.") \
CMD(rd,			s_read,		"[begin [length]] Read memory location.") \
CMD(stack,		s_stack,	"[paint, show] display stack max usage.") \
CMD(mag,		s_mag,		"shows magnetometer readout.") \
CMD(lin,		s_lin,		"shows linear accelerometer readout.") \
CMD(gyro,		s_gyro,		"shows gyrometer readout.") \
CMD(page,		s_page,		"[0-7F] Reads and prints flash page." ) \
CMD(flashfill,	s_flashfill,"[0-7F 0-FFFFFFFF] Fills a flash page with a pattern." ) \
CMD(flasherase, s_flasherase,"[0-7F] Erases flash page." ) \
CMD(printf,		s_printf, 	"write something to printf." ) \
CMD(wear,		s_wear,		"readout wear counters.") \
CMD(mem,		s_mem,		"Gets free heap size of FreeRTOS.") \
CMD(ps,			s_ps,		"Gets the current tasks list.")
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
	shell_tx_str("\033[0;0H" "\033[2J");
}

// Live peripheral viewer. First version shows just GPIO as 0b0000000000000000
void s_live_gpiosec(uint16_t gpio)
{
	for (int i=0; i < 16; i++)
		shell_tx(gpio & (1U << i) ? '1' : '0');
}

void s_live_gpioline()
{
	shell_tx_str("\033[0;0H");
	uint16_t port_a = GPIOA->IDR;  // Input Data Register, 16 bits
	uint16_t port_b = GPIOB->IDR;
	uint16_t port_c = GPIOC->IDR;
	uint16_t port_d = GPIOD->IDR;
	uint16_t port_e = GPIOE->IDR;
	uint16_t port_f = GPIOF->IDR;
	shell_tx_str("GPIO PORTx [0 1 2 ... 14 15]\r\n");
	shell_tx_str("GPIOA: "); 	s_live_gpiosec(port_a);
	shell_tx_str(" GPIOB: "); 	s_live_gpiosec(port_b);
	shell_tx_str(" GPIOC: "); 	s_live_gpiosec(port_c);
	shell_tx_str("\r\nGPIOD: ");s_live_gpiosec(port_d);
	shell_tx_str(" GPIOE: "); 	s_live_gpiosec(port_e);
	shell_tx_str(" GPIOF: "); 	s_live_gpiosec(port_f);
	shell_tx_str("\r\n");
}

void s_live_temp_vbat_vref_line()
{
	int32_t vdda = get_vdda();
	int32_t temp = get_temp();
	int32_t vbat = get_vbat();
	char buf[128];

	sprintf(buf, "Vcc = %4ld [mv] Temperature: %3ld [0.1 degC] Vbat: %4ld [mV]\r\n", vdda, temp, vbat);
	shell_tx_str(buf);
}

void s_adc_line(ADC_HandleTypeDef* adc)
{
	char buf[16];
	for (int i = 1; i <= 18; i++)
	{
		sprintf(buf, "%4ld ", get_adc(adc, i));
		shell_tx_str(buf);
	}
	shell_tx_str("\r\n");
}

void s_line_LSM303AGR()
{
	char buf[128];
	int temp_i = lsm303agr.temp * 10;
	int axi, ayi, azi, mxi, myi, mzi;
	axi = lsm303agr.acc.x*1000; ayi = lsm303agr.acc.y * 1000; azi = lsm303agr.acc.z * 1000;
	mxi = lsm303agr.mag.x*1000; myi = lsm303agr.mag.y * 1000; mzi = lsm303agr.mag.z * 1000;
	sprintf(buf, "LSM303AGR Temp: %4d [0.1 degC] acc xyz %5d %5d %5d [mG] mag xyz %4d %4d %4d [mGauss]\r\n", temp_i, axi, ayi, azi, mxi, myi, mzi);
	shell_tx_str(buf);
}

void s_live(int argc, char **argv)
{
	char c;
	shell_tx_str("\x1b[?25l" "\033[2J"); // Cursur hide, home.
	while ( xQueueReceive(STShell_RXHandle, &c, 0) != pdTRUE )
	{
		s_live_gpioline();
		s_live_temp_vbat_vref_line();
		s_adc_line(&hadc1);
		s_line_LSM303AGR();

		osDelay(250);
	}
	shell_tx_str("\x1b[?25h" "\033[0;0H" "\033[2J"); // Cursor aan, home, clearscreen.
}

void s_read_2(int addr, int length)
{
    // Align op 4 bytes (32-bit read boundary)
    uint32_t aligned = addr & ~3U;
    uint32_t end = aligned + ((length + (addr - aligned) + 3) & ~3U);

    shell_tx_str("\r\n");

    for (uint32_t a = aligned; a < end; a += 32)
    {
        uint8_t buf[32];
        uint32_t remaining = end - a;
        uint32_t line_len = remaining < 32 ? remaining : 32;

        // 32-bit aligned reads, 4 per regel max
        volatile uint32_t *p32 = (volatile uint32_t *)a;
        uint32_t words = (line_len + 3) / 4;

        for (uint32_t w = 0; w < words; w++)
        {
            uint32_t val = p32[w];
            buf[w*4]   = val;
            buf[w*4+1] = val >> 8;
            buf[w*4+2] = val >> 16;
            buf[w*4+3] = val >> 24;
        }

        s_rd_hex_line(a, buf, line_len);
    }
}

void s_read(int argc, char **argv)
{
    if (argc < 2)
    {
        shell_tx_str("Usage: rd <address> [length_bytes]\r\n");
        return;
    }

    uint32_t addr = s_atoi_hex(argv[1]);
    uint32_t length = 0x100;  // default 256 bytes = 4 regels

    if (argc >= 3)
        length = s_atoi_hex(argv[2]);

    //if (length > 256)
    //    length = 256;

    s_read_2(addr, length);
}

extern uint32_t _estack;
extern uint32_t* _Min_Stack_Size;
void s_stack(int argc, char **argv)
{
	if (argc >=2)
	{
		if (strcmp(argv[1], "paint") == 0)
			stack_paint();
		if (strcmp(argv[1], "show") == 0)
			s_read_2(0x10000000, 0x2000);
		return;
	}

	//uint32_t estack = &_estack;
	uint32_t sz = (uint32_t) &_Min_Stack_Size;
    uint32_t *p = (uint32_t *)((uint32_t)&_estack - sz);
    while (*p == 0x23232323) p++;
    uint32_t size =  (uint32_t)&_estack - (uint32_t)p;

    char buf[32];
    sprintf(buf, "Stack: %0ld\r\n", size);
    shell_tx_str(buf);
}

void s_paint(int argc, char **argv)
{
	stack_paint();
}


uint8_t read_mag_reg(uint8_t addr, uint8_t reg)
{
	uint8_t rv;
	HAL_StatusTypeDef hrv;
	hrv = HAL_I2C_Master_Transmit(&hi2c1, addr, &reg, 1, I2C_TIMEOUT);
	if (hrv == HAL_OK)
	{
		hrv = HAL_I2C_Master_Receive(&hi2c1, addr, &rv, 1, I2C_TIMEOUT);
		if (hrv == HAL_OK)
			return rv;
	}
	return 0xFE;
}

void s_mag(int argc, char **argv)
{
	char buf[32];
	for (int i = 0; i < 0x100; i++)
	{
		uint8_t rv = read_mag_reg(LSM303AGR_ADDR_M, i);
		sprintf(buf, "%3X %3X %3X\r\n", LSM303AGR_ADDR_M>>1, i, rv);
		shell_tx_str(buf);
	}
}

void s_lin(int argc, char **argv)
{
	char buf[32];
	for (int i = 0; i < 0x100; i++)
	{
		uint8_t rv = read_mag_reg(LSM303AGR_ADDR_A, i);
		sprintf(buf, "%3X %3X %3X\r\n", LSM303AGR_ADDR_A>>1, i, rv);
		shell_tx_str(buf);
	}
}

// Prints memory content per flash page. Probably writes in the future as well.
void s_page(int argc, char **argv)
{
	const char errstr[] = "Please supply a page number as argument in the range [0-7F]\r\n";
	if (argc <= 1)
	{
		shell_tx_str(errstr);
		return;
	}
	int pagenr = s_atoi_hex(argv[1]);
	if (pagenr < 0 || pagenr > 0x7F)
	{
		shell_tx_str(errstr);
		return;
	}
	s_read_2(FLASH_BASE + pagenr * FLASH_PAGE_SIZE, FLASH_PAGE_SIZE);
}

// Fills a flash page with a given pattern.
void s_flashfill(int argc, char **argv)
{
	const char errstr[] = "Usage: flashfill [pagenr 0-7F] [pattern 0-FFFFFFFF]\r\n";
	if (argc < 3)
	{
		shell_tx_str(errstr);
		return;
	}
	int pagenr = s_atoi_hex(argv[1]);
	if (pagenr > 0x7F)
	{
		shell_tx_str(errstr);
		return;
	}
	uint32_t pattern = s_atoi_hex(argv[2]);
	uint32_t addr = FLASH_BASE + pagenr * FLASH_PAGE_SIZE;

	HAL_FLASH_Unlock();
	for (int i = 0; i < FLASH_PAGE_SIZE / 4; i++)
	{
		if (HAL_OK != HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, pattern))
		{
			shell_tx_str("Programming failed\r\n");
			break;
		}
		addr += 4;
	}
	HAL_FLASH_Lock();
}

// Fills a flash page with a given pattern.
void s_flasherase(int argc, char **argv)
{
	const char errstr[] = "Usage: flasherase [pagenr 0-7F]\r\n";
	if (argc < 2)
	{
		shell_tx_str(errstr);
		return;
	}
	int pagenr = s_atoi_hex(argv[1]);
	if (pagenr > 0x7F)
	{
		shell_tx_str(errstr);
		return;
	}

	FLASH_EraseInitTypeDef ei =
	{
			.TypeErase = FLASH_TYPEERASE_PAGES,
			.PageAddress = FLASH_BASE + pagenr * FLASH_PAGE_SIZE,
			.NbPages = 1,
	};
	uint32_t pageerr = 0;

	HAL_FLASH_Unlock();
	if (HAL_OK != HAL_FLASHEx_Erase(&ei, &pageerr))
	{
		char buf[32];
		sprintf(buf, "Erase failed: %ln\r\n", &pageerr);
		shell_tx_str(buf);
	}
	HAL_FLASH_Lock();
	fc_count_page_erase(pagenr);
}

void s_printf(int argc, char **argv)
{
	for (int i = 1; i < argc; i++)
	{
		if (i > 1)
			printf(" ");
		printf(argv[i]);
	}
	shell_tx_str("\r\n");
}

void s_wear(int argc, char **argv)
{
	char buf[32];
	shell_tx_str("Flash page erase counters.\r\nPage Counter\r\n");
	for (int i = 0; i < PAGE_NUM; i++)
	{
		sprintf(buf, "%4x %5d\r\n", i, get_page_counter(i));
		shell_tx_str(buf);
	}
}

void s_mem(int argc, char **argv)
{
	char buf[32];
	sprintf(buf, "Free heap size: %d\r\n", xPortGetFreeHeapSize());
	shell_tx_str(buf);
	sprintf(buf, "Minimum free heap size: %d\r\n", xPortGetMinimumEverFreeHeapSize());
	shell_tx_str(buf);
}

static const char *task_state_name(eTaskState s)
{
    switch (s)
    {
        case eRunning:    return "Running  ";
        case eReady:      return "Ready    ";
        case eBlocked:    return "Blocked  ";
        case eSuspended:  return "Suspended";
        case eDeleted:    return "Deleted  ";
        case eInvalid:    return "Invalid  ";
        default:          return "?        ";
    }
}

void s_ps(int argc, char **argv)
{
	char buf[128];

	TaskStatus_t stats[10];
	uint32_t total_runtime;
	UBaseType_t n = uxTaskGetSystemState(stats, 10, &total_runtime);
	sprintf(buf, "Runtime: %lu\r\n", total_runtime);
	shell_tx_str(buf);
	shell_tx_str("Name         State     Pri Minstack    Runtime\r\n");
	for (UBaseType_t i = 0; i < n; i++)
	{
		sprintf(buf, "%-12s %s %3lu %8u %10lu\r\n",
                stats[i].pcTaskName,
				task_state_name(stats[i].eCurrentState),   // hmm, zie hieronder
                stats[i].uxCurrentPriority,
                stats[i].usStackHighWaterMark,
				stats[i].ulRunTimeCounter);
		shell_tx_str(buf);
	}
}

void s_gyro(int argc, char **argv)
{
	uint8_t data[0x40];
	char buf[64];
	data[0] = 0x00 | 0x80 | 0x40;		// Set read bit, set auto increment bit.
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi1, &data[0], 1, I2C_TIMEOUT);	// Borrow the I2C timeout for SPI
	HAL_SPI_Receive(&hspi1, data, 0x40, I2C_TIMEOUT);
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_SET);
	for (int i = 0; i < 0x40; i++)
	{
		sprintf(buf, "%3X %3X\r\n", i, data[i]);
		shell_tx_str(buf);
	}
}






