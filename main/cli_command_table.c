/*
 * cli_command_table.c
 *
 *  Created on: May 30, 2023
 */
#include "cli_command_table.h"
#include <stdlib.h>
#include "command_manual.h"
const cli_command_info_t manual_info = {
    .function = command_manual_handler,
    .help = "List all commands and their help"
};
const cli_command_entry_t command_entry_talbe[] =
{
	{"GMR", &manual_info},
	{NULL, NULL}

};
