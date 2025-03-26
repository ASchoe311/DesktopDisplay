//
// Created by adamr on 3/25/25.
//

#ifndef SERIALDATA_H
#define SERIALDATA_H
#endif //SERIALDATA_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include "drivers/lcd/lcd.h"

#define READY_CMD 0x00
#define PAGE_CMD 0x01

#define DATE_CMD 0x02
#define DATE_ROW 0
#define DATE_COL 3

#define TIME_CMD 0x03
#define TIME_ROW 1
#define TIME_COL 4

#define CPU_TEMP_CMD 0x04
#define CPU_TEMP_ROW 0
#define CPU_TEMP_COL 0

#define CPU_USE_CMD 0x05
#define CPU_USE_ROW 0
#define CPU_USE_COL

#define CPU_FAN_SPEED_CMD 0x06
#define GPU_TEMP_CMD 0x07
#define GPU_USE_CMD 0x08
#define GPU_FAN_SPEED_CMD 0x09
#define MEM_USE_CMD 0x0A
#define AUDIO_CMD 0x0B

#define R_PAGE 0x00
#define U_PAGE 0x01
#define D_PAGE 0x02
#define L_PAGE 0x03
#define S_PAGE 0x04

/* Temperature symbol - thermometer */
static uint8_t temperature_char[] = {
    0x0E,  /* 01110 */
    0x0A,  /* 01010 */
    0x0A,  /* 01010 */
    0x0E,  /* 01110 */
    0x0E,  /* 01110 */
    0x1F,  /* 11111 */
    0x1F,  /* 11111 */
    0x0E   /* 01110 */
};

/* Fan symbol - frame 1 (for animation) */
static uint8_t fan_char1[] = {
    0x00,  /* 00000 */
    0x0E,  /* 01110 */
    0x13,  /* 10011 */
    0x15,  /* 10101 */
    0x19,  /* 11001 */
    0x0E,  /* 01110 */
    0x00,  /* 00000 */
    0x00   /* 00000 */
};

/* Fan symbol - frame 2 (for animation) */
static uint8_t fan_char2[] = {
    0x00,  /* 00000 */
    0x0E,  /* 01110 */
    0x19,  /* 11001 */
    0x15,  /* 10101 */
    0x13,  /* 10011 */
    0x0E,  /* 01110 */
    0x00,  /* 00000 */
    0x00   /* 00000 */
};

/* CPU symbol */
static uint8_t cpu_char[] = {
    0x18,  /* 11000 */
    0x10,  /* 10000 */
    0x1B,  /* 11011 */
    0x03,  /* 00011 */
    0x02,  /* 00010 */
    0x02,  /* 00010 */
    0x14,  /* 10100 */
    0x1C   /* 11100 */
};

/* Memory/RAM symbol */
static uint8_t memory_char[] = {
    0x0E,  /* 01110 */
    0x0B,  /* 01011 */
    0x0E,  /* 01110 */
    0x0F,  /* 01111 */
    0x0A,  /* 01010 */
    0x0F,  /* 01111 */
    0x0A,  /* 01010 */
    0x0F   /* 01111 */
};

uint8_t calculate_checksum(uint8_t *data);

bool verify_checksum(uint8_t *data);

void send_message(const struct device *uart_dev, uint8_t *data);

void parse_command_from_ring_buf(struct ring_buf *buf, lcd_state_t *lcd ,uint8_t *cmdByte);

void dispatch_command(lcd_state_t *lcd, uint8_t *command);

void handle_date_cmd(lcd_state_t *lcd, uint8_t *command);

void handle_time_cmd(lcd_state_t *lcd, uint8_t *command);

void handle_song_cmd(lcd_state_t *lcd, uint8_t *command);
