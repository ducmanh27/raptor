/*
 * cli_types.h
 *
 *  Created on: May 30, 2023
 *      Author: Phan Duc Manh
 */

#ifndef CLI_CLI_TYPES_H_
#define CLI_CLI_TYPES_H_

#include <stdint.h>
typedef void (*cli_command_function_t)(char **argv, uint8_t arg_num);
typedef struct
{
	cli_command_function_t 	function;
	char 				    *help;
} cli_command_info_t;

typedef struct
{
	const char *name;
	const cli_command_info_t *command_info;
} cli_command_entry_t;



#endif /* CLI_CLI_TYPES_H_ */
