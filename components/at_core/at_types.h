/*
 * at_types.h
 *
 *  Created on: May 30, 2023
 *      Author: Phan Duc Manh
 */

#ifndef AT_TYPES_H_
#define AT_TYPES_H_

#include <stdint.h>
typedef void (*at_command_function_t)(char **argv, uint8_t arg_num);
typedef struct
{
	at_command_function_t 	function_query;
	at_command_function_t 	function_set;
	at_command_function_t 	function_execute;
	char 				    *help;
} at_command_info_t;

typedef struct
{
	const char *name;
	const at_command_info_t *command_info;
} at_command_entry_t;



#endif /* AT_TYPES_H_ */
