
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
#define BUTTON_NONE_MAX     1023   // Max value for 10-bit resolution

/* Button debouncing parameters */
#define DEBOUNCE_TIME_MS    50      // Time in ms required for stable button reading
#define BUTTON_SAMPLE_MS    10      // Time between button samples/*

/* Device structures */

const struct device *const cdc_dev = DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);

#ifdef DEBUGMODE
const struct device *const uart_dev = DEVICE_DT_GET(DT_NODELABEL(sercom5));
#endif

/* ADC channel from devicetree */
static const struct adc_dt_spec button_adc_channel = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);
/* Define ADC sequence for sampling */
int16_t button_adc_buf;
static struct adc_sequence button_sequence = {
    .buffer = &button_adc_buf,
    .buffer_size = sizeof(button_adc_buf),
};

static const struct adc_dt_spec temp_adc_channel = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 1);
static int16_t temp_adc_buf;
static struct adc_sequence temp_sequence = {
    .buffer = &temp_adc_buf,
    .buffer_size = sizeof(temp_adc_buf),
};

const struct device *porta = DEVICE_DT_GET(DT_NODELABEL(porta));
const struct device *portb = DEVICE_DT_GET(DT_NODELABEL(portb));

const int btn_pin = 20;



/* Initialize the LCD */
static int init_lcd(void)
{
    /* Get GPIO devices */


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

static int init_adc(void) {
    int ret;
    /* Check if ADC controller is ready */
    if (!adc_is_ready_dt(&button_adc_channel)) {
        LOG_ERR("ADC controller device %s not ready", button_adc_channel.dev->name);
        return -1;
    }

    /* Setup ADC channel */
    ret = adc_channel_setup_dt(&button_adc_channel);
    if (ret < 0) {
        LOG_ERR("Could not setup channel #%d (%d)", button_adc_channel.channel_id, ret);
        return -1;
    }

    /* Initialize ADC sequence */
    ret = adc_sequence_init_dt(&button_adc_channel, &button_sequence);
    if (ret < 0) {
        LOG_ERR("Could not initialize sequence (%d)", ret);
        return -1;
    }

    if (!adc_is_ready_dt(&temp_adc_channel)) {
        LOG_ERR("ADC controller device %s not ready", temp_adc_channel.dev->name);
        return -1;
    }

    /* Setup ADC channel */
    ret = adc_channel_setup_dt(&temp_adc_channel);
    if (ret < 0) {
        LOG_ERR("Could not setup channel #%d (%d)", temp_adc_channel.channel_id, ret);
        return -1;
    }

    /* Initialize ADC sequence */
    ret = adc_sequence_init_dt(&temp_adc_channel, &temp_sequence);
    if (ret < 0) {
        LOG_ERR("Could not initialize sequence (%d)", ret);
        return -1;
    }
    return 0;
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
    if (adc_value < 0 || adc_value > 1023) {
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

int await_host() {
    if (await_host_pc(&cdc_rx_rb, &lcd, cdc_dev)){
        LOG_INF("Host PC Ready");
        return 0;
    }
    LOG_ERR("Something went wrong waiting for host PC");
    return -1;
}

static int32_t read_adc_channel(const struct adc_dt_spec *channel, struct adc_sequence *sequence) {
    int ret;

    /* Initialize the sequence for this channel */
    ret = adc_sequence_init_dt(channel, sequence);
    if (ret < 0) {
        LOG_ERR("Could not initialize ADC sequence: %d", ret);
        return -1;
    }

    /* Read the ADC value */
    ret = adc_read_dt(channel, sequence);
    if (ret < 0) {
        LOG_ERR("Could not read ADC: %d", ret);
        return -1;
    }

    /* Return the raw ADC value */
    if (sequence->buffer == &button_adc_buf) {
        return (int32_t)button_adc_buf;
    } else if (sequence->buffer == &temp_adc_buf) {
        return (int32_t)temp_adc_buf;
    }

    return -1;
}

/* Configure ADC for button input (A0) */
static int configure_adc_for_button(void) {
    /* Explicitly reconfigure the ADC for the button channel */
    return adc_channel_setup_dt(&button_adc_channel);
}

/* Configure ADC for temperature input (A4) */
static int configure_adc_for_temp(void) {
    /* Explicitly reconfigure the ADC for the temperature channel */
    return adc_channel_setup_dt(&temp_adc_channel);
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

    ret = init_adc();
    if (ret != 0) {
        LOG_ERR("Failed to initialize ADC: %d", ret);
    }

    // LOG_INF("Testing ADC channels...");
    // while (1) {
    //     test_adc_channels();
    //     k_msleep(10);
    // }

    k_msleep(1000);  // Show initial message for 1 second

    LOG_INF("All devices initialized");

    // Await init command
    if (await_host() == 0){
		LOG_INF("Host PC Ready");
	}
	else {
		LOG_ERR("Something went wrong waiting for host PC");
		return -1;
	}

    lcd_clear(&lcd);
    /* Main loop */
    while (1) {
        /* Read ADC value */
        configure_adc_for_button();
        int32_t raw_value = read_adc_channel(&button_adc_channel, &button_sequence);

        /* If there's data in the ring buffer, process it */
        if (ring_buf_get(&cdc_rx_rb, &byte, 1)) {
            if (!parse_command_from_ring_buf(&cdc_rx_rb, &lcd, &byte) && byte == 0x0E) {
                if (await_host() == 0){
                    LOG_INF("Host PC Ready");
                }
                else {
                    LOG_ERR("Something went wrong waiting for host PC");
                    return -1;
                }
            }
        }

        if (gpio_pin_get(porta, btn_pin)) {
            LOG_INF("Button pin pressed");
        }

        /* Get button state with debouncing */
        current_button = debounce_button(raw_value, &stable_button);

        if (current_button != last_button && current_button != BUTTON_NONE) {
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
        }

        /* Periodically read temperature (every 500ms or so) */
        static uint32_t temp_counter = 0;
        if (++temp_counter >= 50) {
            temp_counter = 0;

            /* Configure and read temperature ADC */
            configure_adc_for_temp();
            int32_t temp_reading = read_adc_channel(&temp_adc_channel, &temp_sequence);

            int err = adc_raw_to_millivolts_dt(&temp_adc_channel, &temp_reading);

            // temp_reading = ((temp_reading / 10.0) * (9.0/5.0)) + 32;

            /* Process temperature reading as needed */
            LOG_INF("Temperature ADC: %dF", temp_reading);

            /* Re-configure for button reading before continuing */
            configure_adc_for_button();
            temp_reading = read_adc_channel(&button_adc_channel, &button_sequence);
            LOG_INF("ADC Value: %dF", temp_reading);
        }

        /* Small delay for button sampling */
        k_msleep(BUTTON_SAMPLE_MS);
    }

    return 0;
}