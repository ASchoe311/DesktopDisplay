/*
 * HD44780-compatible LCD driver for Zephyr RTOS
 * Designed for LCD keypad shield with SPLC780D controller
 */

#ifndef LCD_H
#define LCD_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

/* LCD command codes */
#define LCD_CLEARDISPLAY    0x01
#define LCD_RETURNHOME      0x02
#define LCD_ENTRYMODESET    0x04
#define LCD_DISPLAYCONTROL  0x08
#define LCD_CURSORSHIFT     0x10
#define LCD_FUNCTIONSET     0x20
#define LCD_SETCGRAMADDR    0x40
#define LCD_SETDDRAMADDR    0x80

/* Entry mode flags */
#define LCD_ENTRY_RIGHT     0x00
#define LCD_ENTRY_LEFT      0x02
#define LCD_ENTRY_SHIFT_INC 0x01
#define LCD_ENTRY_SHIFT_DEC 0x00

/* Display control flags */
#define LCD_DISPLAYON       0x04
#define LCD_DISPLAYOFF      0x00
#define LCD_CURSORON        0x02
#define LCD_CURSOROFF       0x00
#define LCD_BLINKON         0x01
#define LCD_BLINKOFF        0x00

/* Function set flags */
#define LCD_8BITMODE        0x10
#define LCD_4BITMODE        0x00
#define LCD_2LINE           0x08
#define LCD_1LINE           0x00
#define LCD_5x10DOTS        0x04
#define LCD_5x8DOTS         0x00

/* Button values (approximate ADC readings) */
#define BUTTON_RIGHT_ADC    0
#define BUTTON_UP_ADC       1
#define BUTTON_DOWN_ADC     2
#define BUTTON_LEFT_ADC     3
#define BUTTON_SELECT_ADC   4
#define BUTTON_NONE_ADC     5

/* Possible button values */
typedef enum {
    BUTTON_NONE,
    BUTTON_RIGHT,
    BUTTON_UP,
    BUTTON_DOWN,
    BUTTON_LEFT,
    BUTTON_SELECT
} lcd_button_t;

/* LCD configuration structure */
typedef struct {
    /* GPIO devices and pins */
    const struct device *rs_gpio_dev;
    gpio_pin_t rs_pin;

    const struct device *enable_gpio_dev;
    gpio_pin_t enable_pin;

    const struct device *d4_gpio_dev;
    gpio_pin_t d4_pin;

    const struct device *d5_gpio_dev;
    gpio_pin_t d5_pin;

    const struct device *d6_gpio_dev;
    gpio_pin_t d6_pin;

    const struct device *d7_gpio_dev;
    gpio_pin_t d7_pin;

    const struct device *backlight_gpio_dev;
    gpio_pin_t backlight_pin;

    /* ADC device and channel for button reading */
    const struct device *adc_dev;
    uint8_t adc_channel;

    /* Display dimensions */
    uint8_t cols;
    uint8_t rows;
} lcd_config_t;

/* LCD state structure */
typedef struct {
    lcd_config_t config;
    uint8_t display_function;
    uint8_t display_control;
    uint8_t display_mode;
    uint8_t current_row;
    uint8_t backlight_state;
} lcd_state_t;

/* Initialize the LCD with the given configuration */
int lcd_init(lcd_state_t *lcd, const lcd_config_t *config);

/* Clear the LCD display */
void lcd_clear(lcd_state_t *lcd);

/* Move cursor to home position */
void lcd_home(lcd_state_t *lcd);

/* Turn on/off the LCD display */
void lcd_display(lcd_state_t *lcd, bool on);

/* Turn on/off the LCD cursor */
void lcd_cursor(lcd_state_t *lcd, bool on);

/* Turn on/off cursor blinking */
void lcd_blink(lcd_state_t *lcd, bool on);

/* Turn on/off the LCD backlight */
void lcd_backlight(lcd_state_t *lcd, bool on);

/* Set cursor position */
void lcd_set_cursor(lcd_state_t *lcd, uint8_t row, uint8_t col);

/* Write a string to the LCD */
void lcd_print(lcd_state_t *lcd, const char *str);

/* Write a single character to the LCD */
void lcd_write_char(lcd_state_t *lcd, char c);

/* Create a custom character (glyph) for use in the LCD */
void lcd_create_char(lcd_state_t *lcd, uint8_t location, uint8_t charmap[]);

/* Read the current button pressed on the shield */
lcd_button_t lcd_read_buttons(lcd_state_t *lcd);

#endif /* LCD_H */