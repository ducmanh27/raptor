#include "at_command_table.h"
#include <stdlib.h>
#include "command_check_version.h"
const at_command_info_t check_version = {
    .function_query = NULL,
    .function_set = NULL,
    .function_execute = check_version_information,
    .help = "AT+GMR Check version infomation"
};
const at_command_entry_t command_entry_talbe[] =
{
	{"GMR", &check_version},
	{NULL, NULL}

};
