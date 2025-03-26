/*
 * HD44780-compatible LCD driver implementation
 * Designed for LCD keypad shield with SPLC780D controller
 */

#include "lcd.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(lcd, LOG_LEVEL_INF);

/* Row offset addresses for different LCD sizes */
static const uint8_t ROW_OFFSETS[] = {0x00, 0x40, 0x14, 0x54};

/* Helper function to pulse the enable pin */
static void lcd_pulse_enable(lcd_state_t *lcd)
{
    gpio_pin_set(lcd->config.enable_gpio_dev, lcd->config.enable_pin, 0);
    k_busy_wait(1);
    gpio_pin_set(lcd->config.enable_gpio_dev, lcd->config.enable_pin, 1);
    k_busy_wait(1);
    gpio_pin_set(lcd->config.enable_gpio_dev, lcd->config.enable_pin, 0);
    k_busy_wait(100);  /* Commands need > 37us to settle */
}

/* Helper function to send 4 bits to the LCD */
static void lcd_write_4bits(lcd_state_t *lcd, uint8_t value)
{
    gpio_pin_set(lcd->config.d4_gpio_dev, lcd->config.d4_pin, (value >> 0) & 0x01);
    gpio_pin_set(lcd->config.d5_gpio_dev, lcd->config.d5_pin, (value >> 1) & 0x01);
    gpio_pin_set(lcd->config.d6_gpio_dev, lcd->config.d6_pin, (value >> 2) & 0x01);
    gpio_pin_set(lcd->config.d7_gpio_dev, lcd->config.d7_pin, (value >> 3) & 0x01);

    lcd_pulse_enable(lcd);
}

/* Send a command to the LCD */
static void lcd_send_command(lcd_state_t *lcd, uint8_t command)
{
    gpio_pin_set(lcd->config.rs_gpio_dev, lcd->config.rs_pin, 0);

    /* Send the high 4 bits */
    lcd_write_4bits(lcd, command >> 4);

    /* Send the low 4 bits */
    lcd_write_4bits(lcd, command & 0x0F);
}

/* Send data to the LCD */
static void lcd_send_data(lcd_state_t *lcd, uint8_t data)
{
    gpio_pin_set(lcd->config.rs_gpio_dev, lcd->config.rs_pin, 1);

    /* Send the high 4 bits */
    lcd_write_4bits(lcd, data >> 4);

    /* Send the low 4 bits */
    lcd_write_4bits(lcd, data & 0x0F);
}

/* Initialize the LCD with the given configuration */
int lcd_init(lcd_state_t *lcd, const lcd_config_t *config)
{
    int ret;

    LOG_INF("Initializing LCD");

    /* Save configuration */
    memcpy(&lcd->config, config, sizeof(lcd_config_t));

    /* Validate GPIO devices */
    if (!device_is_ready(config->rs_gpio_dev) ||
        !device_is_ready(config->enable_gpio_dev) ||
        !device_is_ready(config->d4_gpio_dev) ||
        !device_is_ready(config->d5_gpio_dev) ||
        !device_is_ready(config->d6_gpio_dev) ||
        !device_is_ready(config->d7_gpio_dev)) {
        LOG_ERR("One or more GPIO devices not ready");
        return -ENODEV;
    }

    LOG_INF("Configuring LCD pins");

    /* Set up the pin directions */
    ret = gpio_pin_configure(config->rs_gpio_dev, config->rs_pin, GPIO_OUTPUT);
    if (ret) {
        LOG_ERR("Failed to configure RS pin: %d", ret);
        return ret;
    }

    ret = gpio_pin_configure(config->enable_gpio_dev, config->enable_pin, GPIO_OUTPUT);
    if (ret) {
        LOG_ERR("Failed to configure Enable pin: %d", ret);
        return ret;
    }

    ret = gpio_pin_configure(config->d4_gpio_dev, config->d4_pin, GPIO_OUTPUT);
    if (ret) {
        LOG_ERR("Failed to configure D4 pin: %d", ret);
        return ret;
    }

    ret = gpio_pin_configure(config->d5_gpio_dev, config->d5_pin, GPIO_OUTPUT);
    if (ret) {
        LOG_ERR("Failed to configure D5 pin: %d", ret);
        return ret;
    }

    ret = gpio_pin_configure(config->d6_gpio_dev, config->d6_pin, GPIO_OUTPUT);
    if (ret) {
        LOG_ERR("Failed to configure D6 pin: %d", ret);
        return ret;
    }

    ret = gpio_pin_configure(config->d7_gpio_dev, config->d7_pin, GPIO_OUTPUT);
    if (ret) {
        LOG_ERR("Failed to configure D7 pin: %d", ret);
        return ret;
    }

    /* Configure backlight pin if available */
    if (config->backlight_pin != 0xFF && config->backlight_gpio_dev != NULL) {
        ret = gpio_pin_configure(config->backlight_gpio_dev, config->backlight_pin, GPIO_OUTPUT);
        if (ret) {
            LOG_ERR("Failed to configure Backlight pin: %d", ret);
            return ret;
        }
        /* Turn on backlight by default */
        gpio_pin_set(config->backlight_gpio_dev, config->backlight_pin, 1);
        lcd->backlight_state = 1;
    }

    LOG_INF("Starting LCD initialization sequence according to datasheet");

    /* Initialize LCD according to datasheet */
    lcd->display_function = LCD_4BITMODE | LCD_2LINE | LCD_5x8DOTS;

    /* Wait for more than 40ms after power up */
    LOG_INF("Waiting for LCD power up (50ms)");
    k_msleep(50);

    /* Pull RS low to begin commands */
    gpio_pin_set(config->rs_gpio_dev, config->rs_pin, 0);
    gpio_pin_set(config->enable_gpio_dev, config->enable_pin, 0);

    LOG_INF("Starting 4-bit initialization sequence");

    /* Put the LCD into 4 bit mode */
    /* First write: try to set 8-bit mode first (needed by controller) */
    LOG_INF("LCD init step 1: Set 8-bit mode");
    lcd_write_4bits(lcd, 0x03);
    k_msleep(5);

    /* Second write: try to set 8-bit mode again */
    LOG_INF("LCD init step 2: Set 8-bit mode again");
    lcd_write_4bits(lcd, 0x03);
    k_msleep(5);

    /* Third write: still trying to set 8-bit mode */
    LOG_INF("LCD init step 3: Set 8-bit mode yet again");
    lcd_write_4bits(lcd, 0x03);
    k_busy_wait(150);

    /* Fourth write: finally set to 4-bit mode */
    LOG_INF("LCD init step 4: Finally set 4-bit mode");
    lcd_write_4bits(lcd, 0x02);

    /* Set # of lines, font size, etc. */
    LOG_INF("LCD init: Setting function (lines, font)");
    lcd_send_command(lcd, LCD_FUNCTIONSET | lcd->display_function);

    /* Turn the display on with no cursor or blinking default */
    lcd->display_control = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
    LOG_INF("LCD init: Setting display control");
    lcd_display(lcd, true);

    /* Clear the display */
    LOG_INF("LCD init: Clearing display");
    lcd_clear(lcd);

    /* Set the entry mode */
    lcd->display_mode = LCD_ENTRY_LEFT | LCD_ENTRY_SHIFT_DEC;
    LOG_INF("LCD init: Setting entry mode");
    lcd_send_command(lcd, LCD_ENTRYMODESET | lcd->display_mode);

    LOG_INF("LCD initialization complete");

    return 0;
}

/* Clear the LCD display */
void lcd_clear(lcd_state_t *lcd)
{
    lcd_send_command(lcd, LCD_CLEARDISPLAY);
    k_msleep(2);  /* Clear takes a long time */
}

/* Move cursor to home position */
void lcd_home(lcd_state_t *lcd)
{
    lcd_send_command(lcd, LCD_RETURNHOME);
    k_msleep(2);  /* Return home takes a long time */
}

/* Turn on/off the LCD display */
void lcd_display(lcd_state_t *lcd, bool on)
{
    if (on) {
        lcd->display_control |= LCD_DISPLAYON;
    } else {
        lcd->display_control &= ~LCD_DISPLAYON;
    }
    lcd_send_command(lcd, LCD_DISPLAYCONTROL | lcd->display_control);
}

/* Turn on/off the LCD cursor */
void lcd_cursor(lcd_state_t *lcd, bool on)
{
    if (on) {
        lcd->display_control |= LCD_CURSORON;
    } else {
        lcd->display_control &= ~LCD_CURSORON;
    }
    lcd_send_command(lcd, LCD_DISPLAYCONTROL | lcd->display_control);
}

/* Turn on/off cursor blinking */
void lcd_blink(lcd_state_t *lcd, bool on)
{
    if (on) {
        lcd->display_control |= LCD_BLINKON;
    } else {
        lcd->display_control &= ~LCD_BLINKON;
    }
    lcd_send_command(lcd, LCD_DISPLAYCONTROL | lcd->display_control);
}

/* Turn on/off the LCD backlight */
void lcd_backlight(lcd_state_t *lcd, bool on)
{
    if (lcd->config.backlight_pin != 0xFF && lcd->config.backlight_gpio_dev != NULL) {
        gpio_pin_set(lcd->config.backlight_gpio_dev, lcd->config.backlight_pin, on ? 1 : 0);
        lcd->backlight_state = on ? 1 : 0;
    }
}

/* Set cursor position */
void lcd_set_cursor(lcd_state_t *lcd, uint8_t row, uint8_t col)
{
    if (row >= lcd->config.rows) {
        row = lcd->config.rows - 1;
    }

    lcd->current_row = row;
    lcd_send_command(lcd, LCD_SETDDRAMADDR | (col + ROW_OFFSETS[row]));
}

/* Write a string to the LCD */
void lcd_print(lcd_state_t *lcd, const char *str)
{
    while (*str) {
        lcd_write_char(lcd, *str++);
    }
}

/* Write a single character to the LCD */
void lcd_write_char(lcd_state_t *lcd, char c)
{
    lcd_send_data(lcd, c);
}

/* Create a custom character (glyph) for use in the LCD */
void lcd_create_char(lcd_state_t *lcd, uint8_t location, uint8_t charmap[])
{
    location &= 0x7;  /* Only have 8 locations 0-7 */
    lcd_send_command(lcd, LCD_SETCGRAMADDR | (location << 3));

    for (int i = 0; i < 8; i++) {
        lcd_send_data(lcd, charmap[i]);
    }
}

/* Read the current button pressed on the shield */
lcd_button_t lcd_read_buttons(lcd_state_t *lcd)
{
    /* If ADC device isn't available, no buttons can be read */
    if (!lcd->config.adc_dev || !device_is_ready(lcd->config.adc_dev)) {
        return BUTTON_NONE;
    }

    /* ADC configuration not fully implemented yet */
    /* This would need to set up the ADC channel, take a reading, and convert to button value */
    return BUTTON_NONE;
}