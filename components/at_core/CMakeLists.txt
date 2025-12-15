idf_component_register(SRCS "at_command.c" "at_command_table.c" "command_check_version.c"
                       PRIV_REQUIRES 
                        log
                       INCLUDE_DIRS ".")
