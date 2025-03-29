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
#define CPU_TEMP_COL 2

#define CPU_USE_CMD 0x05
#define CPU_USE_ROW 0
#define CPU_USE_COL 10

#define CPU_FAN_SPEED_CMD 0x06

#define GPU_TEMP_CMD 0x07
#define GPU_TEMP_ROW 0
#define GPU_TEMP_COL 2

#define GPU_USE_CMD 0x08
#define GPU_USE_ROW 0
#define GPU_USE_COL 10

#define GPU_FAN_SPEED_CMD 0x09
#define GPU_FAN_SPEED_ROW 1
#define GPU_FAN_SPEED_COL 0

#define MEM_USE_CMD 0x0A
#define MEM_USE_ROW 1
#define MEM_USE_COL 6

#define AUDIO_CMD 0x0B

#define VRAM_USE_CMD 0x0C
#define VRAM_USE_ROW 1
#define VRAM_USE_COL 10

#define R_PAGE 0x00
#define U_PAGE 0x01
#define D_PAGE 0x02
#define L_PAGE 0x03
#define S_PAGE 0x04



uint8_t calculate_checksum(uint8_t *data);

bool verify_checksum(uint8_t *data);

void send_message(const struct device *uart_dev, uint8_t *data);

bool parse_command_from_ring_buf(struct ring_buf *buf, lcd_state_t *lcd, uint8_t *cmdByte);

void dispatch_command(lcd_state_t *lcd, uint8_t *command);

void handle_date_cmd(lcd_state_t *lcd, uint8_t *command);

void handle_time_cmd(lcd_state_t *lcd, uint8_t *command);

void handle_cpu_temp_cmd(lcd_state_t *lcd, uint8_t *command);

void handle_cpu_usage_cmd(lcd_state_t *lcd, uint8_t *command);

void handle_memory_cmd(lcd_state_t *lcd, uint8_t *command);

void handle_gpu_temp_cmd(lcd_state_t *lcd, uint8_t *command);

void handle_gpu_usage_cmd(lcd_state_t *lcd, uint8_t *command);

void handle_gpu_fan_speed_cmd(lcd_state_t *lcd, uint8_t *command);

void handle_vram_cmd(lcd_state_t *lcd, uint8_t *command);

void handle_song_cmd(lcd_state_t *lcd, uint8_t *command);

void not_implemented_display(lcd_state_t *lcd, uint8_t *command);

bool check_host_ready(uint8_t *command);