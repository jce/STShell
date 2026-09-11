/*
 * shell.h
 *
 *  Created on: Sep 10, 2026
 *      Author: jeindhoven
 */

#ifndef INC_SHELL_H_
#define INC_SHELL_H_

#define SHELL_PROMPT ">"
#define SHELL_VER "0.0.1"

// Xmacro for commands. Members: command, function, helptext
#define COMMANDS \
CMD(help, s_help, "Shows help list") \
CMD(ver, s_version, "Shows versions")

void shell_rx(uint8_t);	// Receive a character. Whole processing hangs on this one function

void shell_register_tx(void (*)(uint8_t)); // Way to supply the shell with the tx function

#endif /* INC_SHELL_H_ */
