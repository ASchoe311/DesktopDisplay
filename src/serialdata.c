//
// Created by adamr on 3/25/25.
//

#include "serialdata.h"
#include <zephyr/drivers/uart.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(serialdata, LOG_LEVEL_DBG);

#define DEBUG false

void log_received_data(uint8_t *command) {
    char dbgBuff[50] = "Received Data (hex): ";
    for (uint8_t i = 0; i < command[1]+3; i++) {
        sprintf(dbgBuff + strlen(dbgBuff), "%02x ", command[i]);
    }
    LOG_DBG("%s", dbgBuff);
}

uint8_t calculate_checksum(uint8_t *data) {
    uint8_t checksum = 0x00;
    for (uint8_t i = 0; i < data[1] + 2; i++) {
      checksum ^= data[i];
    }
    return checksum;
}

bool verify_checksum(uint8_t *data) {
    if (data[1] < 1){
      return false;
    }
    uint8_t received_checksum = data[data[1] + 2];
    uint8_t calc_checksum = calculate_checksum(data);

    return received_checksum == calc_checksum;
}

void send_message(const struct device *uart_dev, uint8_t *data) {

    uint8_t checksum = calculate_checksum(data);
    data[data[1]+2] = checksum;
    LOG_INF("Sending message");

    for(size_t i = 0; i <= data[1] + 2; i++) {
      LOG_INF("Sending %02x", data[i]);
      uart_poll_out(uart_dev, data[i]);
    }
}


bool parse_command_from_ring_buf(struct ring_buf *buf, lcd_state_t *lcd, uint8_t *cmdByte){
    uint8_t dataLength;
    ring_buf_get(buf, &dataLength, 1);
    LOG_INF("Command received: %u", *cmdByte);
    if (*cmdByte == 0x00) {
        if (dataLength == 0x00) {
            return true;
        }
        return false;
    }

    uint8_t data[dataLength+4];
    data[0] = *cmdByte;
    data[1] = dataLength;
    for (uint8_t i = 0; i < dataLength + 1; i++) {
        uint8_t c;
        ring_buf_get(buf, &c, 1);
        data[i+2] = c;
    }
    dispatch_command(lcd, data);
    LOG_INF("Received command");
    return false;
}

void dispatch_command(lcd_state_t *lcd, uint8_t *command) {
    if (!verify_checksum(command)) {
        return;
    }
    LOG_INF("Command verified");
    switch (*command) {
        case DATE_CMD:
            handle_date_cmd(lcd, command);
            break;
        case TIME_CMD:
            handle_time_cmd(lcd, command);
            break;
        case CPU_TEMP_CMD:
            handle_cpu_temp_cmd(lcd, command);
            break;
        case CPU_USE_CMD:
            handle_cpu_usage_cmd(lcd, command);
            break;
        case MEM_USE_CMD:
            handle_memory_cmd(lcd, command);
            break;
        case GPU_TEMP_CMD:
            handle_gpu_temp_cmd(lcd, command);
            break;
        case GPU_USE_CMD:
            handle_gpu_usage_cmd(lcd, command);
            break;
        case GPU_FAN_SPEED_CMD:
            handle_gpu_fan_speed_cmd(lcd, command);
            break;
        case VRAM_USE_CMD:
            handle_vram_cmd(lcd, command);
            break;
        case AUDIO_CMD:
            not_implemented_display(lcd, command);
            break;
        default: return;
    }
}

void handle_date_cmd(lcd_state_t *lcd, uint8_t *command) {

    if (DEBUG) {
        log_received_data(command);
    }

    lcd_set_cursor(lcd, DATE_ROW, DATE_COL);
    char printStr[14];
    sprintf(printStr, "%02u/%02u/%u", command[2], command[3], (command[4] << 8) | command[5]);
    LOG_INF("Received date: %s", printStr);
    lcd_print(lcd, printStr);
}

void handle_time_cmd(lcd_state_t *lcd, uint8_t *command) {

    if (DEBUG) {
        log_received_data(command);
    }

    lcd_set_cursor(lcd, TIME_ROW, TIME_COL);
    char printStr[11] = "HH:MM AA";
    char amPm[3];
    if (command[4]){
        sprintf(amPm, "PM");
    }
    else{
        sprintf(amPm, "AM");
    }
    sprintf(printStr, "%02u:%02u %s", command[2], command[3], amPm);
    LOG_INF("Received time: %s", printStr);
    lcd_print(lcd, printStr);
}

void handle_cpu_temp_cmd(lcd_state_t *lcd, uint8_t *command) {
    lcd_set_cursor(lcd, CPU_TEMP_ROW, CPU_TEMP_COL);
    lcd_write_char(lcd, 0);
    lcd_set_cursor(lcd, CPU_TEMP_ROW, CPU_TEMP_COL + 1);
    char tempStr[10] = {0};
    sprintf(tempStr, "%dC", command[2]);
    LOG_INF("Received CPU temperature: %s", tempStr);
    lcd_print(lcd, tempStr);
}

void handle_cpu_usage_cmd(lcd_state_t *lcd, uint8_t *command) {
    lcd_set_cursor(lcd, CPU_USE_ROW, CPU_USE_COL);
    lcd_write_char(lcd, 2);
    lcd_set_cursor(lcd, CPU_USE_ROW, CPU_USE_COL + 1);
    char useStr[10] = {0};
    sprintf(useStr, "%02d%%", command[2]);
    LOG_INF("Received CPU usage: %s", useStr);
    lcd_print(lcd, useStr);
}

void handle_memory_cmd(lcd_state_t *lcd, uint8_t *command) {
    lcd_set_cursor(lcd, MEM_USE_ROW, MEM_USE_COL);
    lcd_write_char(lcd, 1);
    lcd_set_cursor(lcd, MEM_USE_ROW, MEM_USE_COL + 1);
    char memStr[10] = {0};
    sprintf(memStr, "%d%%", command[2]);
    LOG_INF("Received memory usage: %s", memStr);
    lcd_print(lcd, memStr);
}

void handle_gpu_temp_cmd(lcd_state_t *lcd, uint8_t *command) {
    lcd_set_cursor(lcd, GPU_TEMP_ROW, GPU_TEMP_COL);
    lcd_write_char(lcd, 0);
    lcd_set_cursor(lcd, GPU_TEMP_ROW, GPU_TEMP_COL + 1);
    char tempStr[10] = {0};
    sprintf(tempStr, "%dC", command[2]);
    LOG_INF("Received GPU temperature: %s", tempStr);
    lcd_print(lcd, tempStr);
}

void handle_gpu_usage_cmd(lcd_state_t *lcd, uint8_t *command) {
    lcd_set_cursor(lcd, GPU_USE_ROW, GPU_USE_COL);
    lcd_write_char(lcd, 2);
    lcd_set_cursor(lcd, GPU_USE_ROW, GPU_USE_COL + 1);
    char useStr[10] = {0};
    sprintf(useStr, "%02d%%", command[2]);
    LOG_INF("Received GPU usage: %s", useStr);
    lcd_print(lcd, useStr);
}

void handle_gpu_fan_speed_cmd(lcd_state_t *lcd, uint8_t *command) {
    lcd_set_cursor(lcd, GPU_FAN_SPEED_ROW, GPU_FAN_SPEED_COL);
    // if (fan_icon_switch) {
    //     lcd_write_char(lcd, 3);
    // }
    // else {
    //     lcd_write_char(lcd, 4);
    // }
    // fan_icon_switch = !fan_icon_switch;
    lcd_write_char(lcd, 3);
    lcd_set_cursor(lcd, GPU_FAN_SPEED_ROW, GPU_FAN_SPEED_COL + 1);
    char speedStr[16] = {0};
    sprintf(speedStr, "%dRPM", command[2] << 8 | command[3]);
    LOG_INF("Received GPU fan speed: %s", speedStr);
    lcd_print(lcd, speedStr);
}

void handle_vram_cmd(lcd_state_t *lcd, uint8_t *command) {
    lcd_set_cursor(lcd, VRAM_USE_ROW, VRAM_USE_COL);
    lcd_write_char(lcd, 1);
    lcd_set_cursor(lcd, VRAM_USE_ROW, VRAM_USE_COL + 1);
    char vramStr[10] = {0};
    sprintf(vramStr, "%02d%%", command[2]);
    LOG_INF("Received VRAM usage: %s", vramStr);
    lcd_print(lcd, vramStr);
}

void handle_song_cmd(lcd_state_t *lcd, uint8_t *command) {

    if (DEBUG) {
        log_received_data(command);
    }

    lcd_set_cursor(lcd, 0, 0);
    char printStr[17];
    for (uint8_t i = 2; i < command[1]+2; i++) {
        sprintf(printStr + strlen(printStr), "%c", command[i]);
    }
    LOG_INF("Received song: %s", printStr);
    lcd_print(lcd, printStr);
}

void not_implemented_display(lcd_state_t *lcd, uint8_t *command) {
    lcd_set_cursor(lcd, 0, 0);
    char printStr[19] = {0};
    for (uint8_t i = 2; i < command[1]+1; i++) {
        sprintf(printStr + strlen(printStr), "%c", command[i]);
    }

    LOG_INF("Received data (str): %s", printStr);
    lcd_print(lcd, printStr);

    lcd_set_cursor(lcd, 1, 0);
    sprintf(printStr, "Page: %u", command[command[1] + 1]);
    char c;
    switch (command[command[1] + 1]) {
        case 0x01:
            c = '1';
            break;
        case 0x02:
            c = '2';
            break;
        case 0x03:
            c = '3';
            break;
        default:
            c = '0';
            break;
    }
    lcd_write_char(lcd, c);
}


