//
// Created by adamr on 3/25/25.
//

#include "serialdata.h"
#include <zephyr/drivers/uart.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(serialdata, LOG_LEVEL_DBG);

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


void parse_command_from_ring_buf(struct ring_buf *buf, lcd_state_t *lcd, uint8_t *cmdByte){
    uint8_t dataLength;
    ring_buf_get(buf, &dataLength, 1);
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
}

void dispatch_command(lcd_state_t *lcd, uint8_t *command) {
    if (!verify_checksum(command)) {
        return;
    }
    LOG_INF("Command verified");
    switch (*command) {
        case 0x02:
          handle_date_cmd(lcd, command);
          break;
        case 0x03:
          handle_time_cmd(lcd, command);
          break;
        case 0x0B:
          handle_song_cmd(lcd, command);
        default: return;
    }
}

void handle_date_cmd(lcd_state_t *lcd, uint8_t *command) {

//    char dbgBuff[50] = "Received Date Data: ";
//    for (uint8_t i = 0; i < command[1]+3; i++) {
//        sprintf(dbgBuff + strlen(dbgBuff), "%02x ", command[i]);
//    }
//    LOG_INF("%s", dbgBuff);

    lcd_set_cursor(lcd, DATE_ROW, DATE_COL);
    char printStr[14];
    sprintf(printStr, "%02u/%02u/%u", command[2], command[3], (command[4] << 8) | command[5]);
    LOG_INF("Received date: %s", printStr);
    lcd_print(lcd, printStr);
}

void handle_time_cmd(lcd_state_t *lcd, uint8_t *command) {

//    char dbgBuff[50] = "Received Time Data: ";
//    for (uint8_t i = 0; i < command[1]+3; i++) {
//        sprintf(dbgBuff + strlen(dbgBuff), "%02x ", command[i]);
//    }
//    LOG_INF("%s", dbgBuff);

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

void handle_song_cmd(lcd_state_t *lcd, uint8_t *command) {

    char dbgBuff[50] = "Received Music Data: ";
    for (uint8_t i = 0; i < command[1]+3; i++) {
        sprintf(dbgBuff + strlen(dbgBuff), "%02x ", command[i]);
    }
    LOG_INF("%s", dbgBuff);

    lcd_set_cursor(lcd, 0, 0);
    char printStr[17];
    for (uint8_t i = 2; i < command[1]+2; i++) {
        sprintf(printStr + strlen(printStr), "%c", command[i]);
    }
    sprintf(printStr + strlen(printStr), "%02x", command[1]+2);
    LOG_INF("Received song: %s", printStr);
    lcd_print(lcd, printStr);
}