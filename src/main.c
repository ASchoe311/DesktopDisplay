
 /* LCD Keypad Display for Arduino MKR Zero using Zephyr RTOS
 *
 * Displays the most recently pressed button on the LCD
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/drivers/uart/cdc_acm.h>
#include <zephyr/drivers/adc.h>
#include <string.h>
#include "drivers/lcd/lcd.h"
#include "serialdata.h"

#ifndef DEBUGMODE
#define DEBUGMODE
#endif

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

/* LCD state */
static lcd_state_t lcd;

/* Size of the ring buffer for CDC ACM RX */
#define RING_BUF_SIZE 256

/* Create a ring buffer for storing received data */
RING_BUF_DECLARE(cdc_rx_rb, RING_BUF_SIZE);

/* Button value ranges based on observed values with pull-down resistor */
#define BUTTON_RIGHT_MAX    200
#define BUTTON_UP_MAX       400
#define BUTTON_DOWN_MAX     550
#define BUTTON_LEFT_MAX     650
#define BUTTON_SELECT_MAX   745
#define BUTTON_NONE_MAX     1023   // Max value for 12-bit resolution

/* Button debouncing parameters */
#define DEBOUNCE_TIME_MS    50      // Time in ms required for stable button reading
#define BUTTON_SAMPLE_MS    10      // Time between button samples/*

/* Device structures */

const struct device *const cdc_dev = DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);

#ifdef DEBUGMODE
const struct device *const uart_dev = DEVICE_DT_GET(DT_NODELABEL(sercom5));
#endif

/* ADC channel from devicetree */
static const struct adc_dt_spec adc_channel = ADC_DT_SPEC_GET(DT_PATH(zephyr_user));

/* Initialize the LCD */
static int init_lcd(void)
{
    /* Get GPIO devices */
    const struct device *porta = DEVICE_DT_GET(DT_NODELABEL(porta));
    const struct device *portb = DEVICE_DT_GET(DT_NODELABEL(portb));

    if (!device_is_ready(porta) || !device_is_ready(portb)) {
        LOG_ERR("GPIO devices not ready");
        return -ENODEV;
    }

    /* Configure LCD pins according to your wiring:
     * D0 (PA22) -> LCD D4
     * D1 (PA23) -> LCD D5
     * D2 (PA10) -> LCD D6
     * D3 (PA11) -> LCD D7
     * D4 (PB10) -> LCD RS
     * D5 (PB11) -> LCD Enable
     */
    lcd_config_t config = {
        /* RS pin - using PB10 (D4) */
        .rs_gpio_dev = portb,
        .rs_pin = 10,

        /* Enable pin - using PB11 (D5) */
        .enable_gpio_dev = portb,
        .enable_pin = 11,

        /* Data pins - all on PORTA */
        .d4_gpio_dev = porta,
        .d4_pin = 22,  /* D0 */

        .d5_gpio_dev = porta,
        .d5_pin = 23,  /* D1 */

        .d6_gpio_dev = porta,
        .d6_pin = 10,  /* D2 */

        .d7_gpio_dev = porta,
        .d7_pin = 11,  /* D3 */

        /* No separate backlight control pin */
        .backlight_gpio_dev = NULL,
        .backlight_pin = 0xFF,

        /* ADC for buttons will be set up separately */
        .adc_dev = NULL,
        .adc_channel = 0,

        /* LCD dimensions - standard 16x2 LCD */
        .cols = 16,
        .rows = 2
    };

    /* Initialize LCD */
    int ret = lcd_init(&lcd, &config);
    if (ret == 0) {
        /* Show a welcome message */
        lcd_clear(&lcd);
        lcd_print(&lcd, "LCD Initialized");
        lcd_set_cursor(&lcd, 1, 0);
    }

    lcd_create_char(&lcd, 0, temperature_char);
    lcd_create_char(&lcd, 1, memory_char);
    lcd_create_char(&lcd, 2, cpu_char);
    lcd_create_char(&lcd, 3, fan_char1);
    lcd_create_char(&lcd, 4, fan_char2);

    return ret;
}



/* UART interrupt callback function */
static void cdc_cb(const struct device *dev, void *user_data)
{
    uint8_t byte;

    /* Process all available data in the CDC FIFO */
    while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
        /* If RX data is available, read it */
        if (uart_irq_rx_ready(dev)) {
            /* Read a single byte */
            if (uart_fifo_read(dev, &byte, 1) == 1) {
                /* Add the byte to our ring buffer */
                ring_buf_put(&cdc_rx_rb, &byte, 1);
            }
        }
    }
}

/* Identify which button is pressed based on ADC value */
static lcd_button_t identify_button(int32_t adc_value)
{
    if (adc_value <= BUTTON_RIGHT_MAX) {
        return BUTTON_RIGHT;
    } else if (adc_value <= BUTTON_UP_MAX) {
        return BUTTON_UP;
    } else if (adc_value <= BUTTON_DOWN_MAX) {
        return BUTTON_DOWN;
    } else if (adc_value <= BUTTON_LEFT_MAX) {
        return BUTTON_LEFT;
    } else if (adc_value <= BUTTON_SELECT_MAX) {
        return BUTTON_SELECT;
    } else {
        return BUTTON_NONE; /* Default to NONE if no other button matches */
    }
}

/* Advanced debounced button detection function */
static lcd_button_t debounce_button(int32_t adc_value, lcd_button_t *last_stable_button)
{
    static lcd_button_t current_button = BUTTON_NONE;
    static lcd_button_t last_button = BUTTON_NONE;
    static uint32_t stable_since = 0;
    static uint8_t stability_counter = 0;
    uint32_t now = k_uptime_get_32();

    /* First, verify the ADC value is within valid overall range */
    if (adc_value < 0 || adc_value > 4095) {
        /* Invalid ADC reading - reject it */
        return *last_stable_button;
    }

    /* Get raw button state from ADC value */
    lcd_button_t raw_button = identify_button(adc_value);

    /* Check if button state changed */
    if (raw_button != last_button) {
        /* Button state changed, reset the stable timer and counter */
        last_button = raw_button;
        stable_since = now;
        stability_counter = 0;
        return *last_stable_button; /* Return last stable state while unstable */
    }

    /* If button state has been stable for DEBOUNCE_TIME_MS */
    if ((now - stable_since) >= DEBOUNCE_TIME_MS) {
        /* Increment stability counter */
        if (stability_counter < 255) {
            stability_counter++;
        }

        /* Only accept state as stable after multiple consistent readings */
        if (stability_counter >= 3) {
            /* We have a stable button state */
            if (raw_button != current_button) {
                current_button = raw_button;
                *last_stable_button = current_button;
            }
        }
    }

    return *last_stable_button;
}

uint8_t btn_map(lcd_button_t button) {
    switch (button) {
        case BUTTON_RIGHT: return 0x00;
        case BUTTON_UP:    return 0x01;
        case BUTTON_DOWN:  return 0x02;
        case BUTTON_LEFT:  return 0x03;
        case BUTTON_SELECT: return 0x04;
        default: return 0x00;
    }
}


int main(void)
{
    uint8_t byte;
    int ret;
    lcd_button_t current_button = BUTTON_NONE;
    lcd_button_t last_button = BUTTON_NONE;
    lcd_button_t stable_button = BUTTON_NONE;  // For debouncing
    bool diagnostic_mode = false;  // Set to true to show raw ADC values

#ifdef DEBUGMODE
    if (!device_is_ready(uart_dev)) {
        LOG_ERR("UART device (SERCOM5) not ready");
        return -1;
    }
#endif
    /* Initialize USB CDC ACM - proper device tree approach */
    if (!device_is_ready(cdc_dev)) {
        LOG_ERR("CDC ACM device not ready");
        return -1;
    }

    /* Configure CDC ACM interrupt callback */
    uart_irq_callback_set(cdc_dev, cdc_cb);

    /* Enable CDC ACM RX interrupt */
    uart_irq_rx_enable(cdc_dev);

    ret = usb_enable(NULL);
    if (ret != 0) {
        LOG_ERR("Failed to enable USB");
        return -1;
    }

    /* Initialize LCD */
    ret = init_lcd();
    if (ret != 0) {
        LOG_ERR("Failed to initialize LCD: %d", ret);
        return -1;
    }

    /* Check if ADC controller is ready */
    if (!adc_is_ready_dt(&adc_channel)) {
        LOG_ERR("ADC controller device %s not ready", adc_channel.dev->name);
        return -1;
    }

    /* Setup ADC channel */
    ret = adc_channel_setup_dt(&adc_channel);
    if (ret < 0) {
        LOG_ERR("Could not setup channel #%d (%d)", adc_channel.channel_id, ret);
        return -1;
    }

    /* Define ADC sequence for sampling */
    int16_t adc_buf;
    struct adc_sequence sequence = {
        .buffer = &adc_buf,
        .buffer_size = sizeof(adc_buf),
    };

    /* Initialize ADC sequence */
    ret = adc_sequence_init_dt(&adc_channel, &sequence);
    if (ret < 0) {
        LOG_ERR("Could not initialize sequence (%d)", ret);
        return -1;
    }

    k_msleep(1000);  // Show initial message for 1 second

    lcd_clear(&lcd);
    lcd_home(&lcd);
    lcd_print(&lcd, "Device Ready");
    lcd_set_cursor(&lcd, 1, 0);
    lcd_print(&lcd, "Awaiting Host PC");
    LOG_INF("All devices initialized");
    LOG_INF("Awaiting host PC initialization command");

    // Await init command
    while (1) {
        LOG_INF("Waiting for command");
        if (ring_buf_get(&cdc_rx_rb, &byte, 1)) {
            LOG_INF("Buffer has bytes");
            if (parse_command_from_ring_buf(&cdc_rx_rb, &lcd, &byte)) {
                LOG_INF("Got ready command from host PC");
                lcd_clear(&lcd);
                lcd_home(&lcd);
                lcd_print(&lcd, "Host Ready");
                lcd_set_cursor(&lcd, 1, 0);
                lcd_print(&lcd, "Sending ready");
                LOG_INF("Sending ready message back to host");
                const uint8_t readyCmd[2] = {0x00, 0x00};
                for (size_t i = 0; i < 2; i++) {
                    uart_poll_out(cdc_dev, readyCmd[i]);
                }
                uint8_t cmd[4] = {
                    0x01,
                    0x01,
                    0x00,
                    0x00
                };
                send_message(cdc_dev, cmd);
                break;
            }
        }
        k_msleep(500);
    }

    lcd_clear(&lcd);
    /* Main loop */
    while (1) {
        /* Read ADC value */
        (void)adc_sequence_init_dt(&adc_channel, &sequence);
        int err = adc_read_dt(&adc_channel, &sequence);
        if (err < 0) {
            LOG_ERR("Could not read ADC (%d)", ret);
            k_msleep(100);
            continue;
        }

        /* If there's data in the ring buffer, process it */
        if (ring_buf_get(&cdc_rx_rb, &byte, 1)) {
            parse_command_from_ring_buf(&cdc_rx_rb, &lcd, &byte);
        }


        /* Get raw ADC value */
        int32_t raw_value;
        raw_value = (int32_t)adc_buf;
        /* Get button state with debouncing */
        current_button = debounce_button(raw_value, &stable_button);


        if (current_button != last_button && current_button != BUTTON_NONE) {
            /* Clear the second line */
            // lcd_clear(&lcd);
            // lcd_print(&lcd, "                ");
            //
            // /* Print the button name */
            // lcd_set_cursor(&lcd, 0, 0);
            // lcd_print(&lcd, "Key: ");
            // lcd_write_char(&lcd, button_name(current_button));
            //
            // /* Print the raw ADC value for debugging */
            // lcd_set_cursor(&lcd, 0, 1);
            // char adc_str[16];
            // snprintf(adc_str, sizeof(adc_str), "ADC:%d", raw_value);
            // lcd_print(&lcd, adc_str);

            last_button = current_button;

            /* Log the button press */
            uint8_t cmd[4] = {
                0x01,
                0x01,
                btn_map(current_button),
                0x00
            };

            LOG_INF("Sending command");
            LOG_INF("current button is: %d", cmd[2]);

            send_message(cdc_dev, cmd);
            lcd_clear(&lcd);
            ring_buf_reset(&cdc_rx_rb);
            // LOG_INF("Button: %s, ADC: %d", button_name(current_button), raw_value);
        }

        /* Small delay for button sampling */
        k_msleep(BUTTON_SAMPLE_MS);
    }

    return 0;
}