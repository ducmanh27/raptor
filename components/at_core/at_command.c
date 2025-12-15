/* ==================== IMPLEMENTATION ==================== */

#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include "at_command.h"
#include "at_command_table.h"

#include "esp_log.h"
#include "at_command_table.h"

static const char *TAG = "at_command";

void at_parser_init(at_parser_state_t *parser) {
    memset(parser->buffer, 0, sizeof(parser->buffer));
    parser->index = 0;
    parser->in_command = false;
}

bool at_parser_process_byte(at_parser_state_t *parser, uint8_t byte, at_command_t *cmd) {
    // Kiểm tra bắt đầu AT command
    if (!parser->in_command) {
        if (parser->index == 0 && byte == 'A') {
            parser->buffer[parser->index++] = byte;
            return false;
        }
        else if (parser->index == 1 && byte == 'T') {
            parser->buffer[parser->index++] = byte;
            return false;
        }
        else if (parser->index == 2 && byte == '+') {
            parser->buffer[parser->index++] = byte;
            parser->in_command = true;
            return false;
        }
        else {
            // Reset nếu không phải AT+
            at_parser_init(parser);
            return false;
        }
    }
    
    // Đang trong command, kiểm tra kết thúc
    if (parser->in_command) {
        // Kiểm tra CR (\r = 0x0D)
        if (byte == '\r') {
            parser->buffer[parser->index++] = byte;
            return false;
        }
        // Kiểm tra LF (\n = 0x0A) - kết thúc command
        else if (byte == '\n' && parser->index > 0 && parser->buffer[parser->index - 1] == '\r') {
            parser->buffer[parser->index] = '\0';  // Null terminate
            
            // Parse command (bỏ qua "AT+" và "\r")
            char cmd_str[AT_CMD_MAX_LENGTH];
            int cmd_len = parser->index - 4;  // Bỏ "AT+" và "\r\n"
            if (cmd_len > 0) {
                strncpy(cmd_str, parser->buffer + 3, cmd_len);
                cmd_str[cmd_len] = '\0';
                
                // Parse thành structure
                bool result = at_parse_command(cmd_str, cmd);
                
                // Reset parser
                at_parser_init(parser);
                
                return result;
            }
            
            at_parser_init(parser);
            return false;
        }
        // Ký tự bình thường
        else {
            if (parser->index < AT_CMD_MAX_LENGTH - 1) {
                parser->buffer[parser->index++] = byte;
            }
            return false;
        }
    }
    
    return false;
}

// Hàm loại bỏ dấu ngoặc kép
static void remove_quotes(char *str) {
    int len = strlen(str);
    if (len >= 2 && str[0] == '"' && str[len-1] == '"') {
        memmove(str, str + 1, len - 2);
        str[len - 2] = '\0';
    }
}

bool at_parse_command(const char *cmd_str, at_command_t *cmd) {
    if (!cmd_str || !cmd) {
        return false;
    }
    
    memset(cmd, 0, sizeof(at_command_t));
    
    // Tìm vị trí của '=', '?' để xác định loại lệnh
    const char *equal_pos = strchr(cmd_str, '=');
    const char *question_pos = strchr(cmd_str, '?');
    
    // Xác định loại lệnh và tên lệnh
    int cmd_name_len = 0;
    
    if (equal_pos) {
        // AT+CMD=? (TEST) hoặc AT+CMD=<params> (SET)
        cmd_name_len = equal_pos - cmd_str;
        
        if (equal_pos[1] == '?') {
            cmd->type = AT_CMD_TYPE_TEST;
        } else {
            cmd->type = AT_CMD_TYPE_SET;
            
            // Parse parameters
            const char *param_start = equal_pos + 1;
            char temp_params[AT_CMD_MAX_LENGTH];
            strncpy(temp_params, param_start, sizeof(temp_params) - 1);
            temp_params[sizeof(temp_params) - 1] = '\0';
            
            // Tách parameters bằng dấu phẩy
            char *token = temp_params;
            char *next_token = NULL;
            bool in_quotes = false;
            int start = 0;
            
            for (int i = 0; temp_params[i] != '\0' && cmd->param_count < AT_CMD_MAX_PARAMS; i++) {
                if (temp_params[i] == '"') {
                    in_quotes = !in_quotes;
                }
                else if (temp_params[i] == ',' && !in_quotes) {
                    temp_params[i] = '\0';
                    strncpy(cmd->params[cmd->param_count], token + start, AT_PARAM_MAX_LENGTH - 1);
                    cmd->params[cmd->param_count][AT_PARAM_MAX_LENGTH - 1] = '\0';
                    remove_quotes(cmd->params[cmd->param_count]);
                    cmd->param_count++;
                    start = i + 1;
                    token = temp_params + start;
                }
            }
            
            // Thêm parameter cuối cùng
            if (token[0] != '\0' && cmd->param_count < AT_CMD_MAX_PARAMS) {
                strncpy(cmd->params[cmd->param_count], token, AT_PARAM_MAX_LENGTH - 1);
                cmd->params[cmd->param_count][AT_PARAM_MAX_LENGTH - 1] = '\0';
                remove_quotes(cmd->params[cmd->param_count]);
                cmd->param_count++;
            }
        }
    }
    else if (question_pos) {
        // AT+CMD? (QUERY)
        cmd_name_len = question_pos - cmd_str;
        cmd->type = AT_CMD_TYPE_QUERY;
    }
    else {
        // AT+CMD (EXECUTE)
        cmd_name_len = strlen(cmd_str);
        cmd->type = AT_CMD_TYPE_EXECUTE;
    }
    
    // Copy tên lệnh
    if (cmd_name_len > 0 && cmd_name_len < sizeof(cmd->cmd)) {
        strncpy(cmd->cmd, cmd_str, cmd_name_len);
        cmd->cmd[cmd_name_len] = '\0';
    } else {
        return false;
    }
    
    return true;
}


const at_command_info_t* find_command_infor(char *cmd)
{
	const at_command_entry_t *command_entry = command_entry_talbe;
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

void at_command_execute(at_command_t *cmd)
{
	const at_command_info_t *command_infor = find_command_infor(cmd->cmd);

    char *argv[AT_CMD_MAX_LENGTH];
    uint8_t arg_num = cmd->param_count;
    for (int i = 0; i < arg_num; i++)
    {
        argv[i] = cmd->params[i];
    }

	if (command_infor != NULL)
	{
        switch(cmd->type) {
        case AT_CMD_TYPE_QUERY:
            ESP_LOGI(TAG,"QUERY (?)");
            if (command_infor->function_query)
                command_infor->function_query(argv, arg_num);
            break;
        case AT_CMD_TYPE_SET:
            ESP_LOGI(TAG, "SET (=<params>)");
            ESP_LOGI(TAG, "Parameters (%d):", cmd->param_count);
            for (int i = 0; i < cmd->param_count; i++) {
                ESP_LOGI(TAG, "  [%d]: %s", i, cmd->params[i]);
            }
            if (command_infor->function_set)
                command_infor->function_set(argv, arg_num);
            break;
        case AT_CMD_TYPE_EXECUTE:
            ESP_LOGI(TAG,"EXECUTE");
            if (command_infor->function_execute)
                command_infor->function_execute(argv, arg_num);
            break;
        default:
            break;
        }
	}
	else
	{
		ESP_LOGE(TAG, "Command %s not found", cmd->cmd);
	}

}
