/* 
 * AT Command Parser Module
 * Author: Phan Duc Manh
 * License: MIT License
 */

#ifndef AT_COMMAND_H
#define AT_COMMAND_H

#include <stdint.h>
#include <stdbool.h>

#define AT_CMD_MAX_LENGTH 256
#define AT_CMD_MAX_PARAMS 10
#define AT_PARAM_MAX_LENGTH 64

typedef enum {
    AT_CMD_TYPE_TEST,       // AT+CMD=?
    AT_CMD_TYPE_QUERY,      // AT+CMD?
    AT_CMD_TYPE_SET,        // AT+CMD=<params>
    AT_CMD_TYPE_EXECUTE     // AT+CMD
} at_cmd_type_t;

typedef struct {
    char cmd[32];                           // Tên lệnh (VD: "GMR", "CIPMUX")
    at_cmd_type_t type;                     // Loại lệnh
    char params[AT_CMD_MAX_PARAMS][AT_PARAM_MAX_LENGTH];  // Mảng parameters
    uint8_t param_count;                    // Số lượng parameters
} at_command_t;

typedef struct {
    char buffer[AT_CMD_MAX_LENGTH];
    uint16_t index;
    bool in_command;
} at_parser_state_t;

// Khởi tạo parser
void at_parser_init(at_parser_state_t *parser);

// Xử lý từng byte nhận được
bool at_parser_process_byte(at_parser_state_t *parser, uint8_t byte, at_command_t *cmd);

// Parse command string thành cấu trúc at_command_t
bool at_parse_command(const char *cmd_str, at_command_t *cmd);

void at_command_execute(at_command_t *cmd);

#endif // AT_COMMAND_H
