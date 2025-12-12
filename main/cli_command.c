/*
 * cli_command.c
 *
 *  Created on: May 30, 2023
 *      Author: Phan Duc Manh
 */
#include "cli_command.h"
#include <stdio.h>
#include "cli_command_table.h"


const cli_command_info_t* find_command_infor(char *cmd)
{
	const cli_command_entry_t *command_entry = command_entry_talbe;
	while (command_entry->command_info != NULL)
	{
		if (strcmp(command_entry->name, cmd) == 0)
		{
			 return command_entry->command_info;
		}

		command_entry++;
	}
	return NULL;

}

void cli_command_excute(char *uart_buff, uint8_t length)
{
	if (uart_buff == NULL) return ;
	if (uart_buff[0] == '{') return ;
	char *argv[20];
	uint8_t arg_num = 0;
	char *token;
	token = strtok(uart_buff, " ");
	while (token != NULL)
	{
		argv[arg_num++] = token;
		token = strtok(NULL, " ");
	}
	const cli_command_info_t *command_infor = find_command_infor(argv[0]);
	if (command_infor != NULL)
	{
		command_infor->function(argv, arg_num);
	}
	else
	{
			printf("Command %s not found \n", argv[0]);
	}

}
