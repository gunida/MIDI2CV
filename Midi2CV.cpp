#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/pwm.h"

#define UART_ID uart1
#define UART_RX_PIN 5
#define BAUD_RATE 31250
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY UART_PARITY_NONE

#define NOTE_ON 0x90
#define NOTE_OFF 0x80
#define MIDI_CLK 0xF8

#define CFG_BUTTON_GPIO 0
#define LED_DELAY_MS 80

typedef struct
{
    short midi_channel;
    short gate_pin;
} ChannelToPinMapping;

void gpio_callback(uint gpio, uint32_t events);
void on_uart_rx();
void setup();
void midi_config();
void set_config_mode(bool enabled);
void run();
unsigned char midi_rx();
void print_midi_msg(char status, char data_1, char data_2);
void blink_led(int num_times, int delay_nms);
void gate_out(int gpio, bool on);
void note_out(int gpio, int note);

unsigned char status = 0;
unsigned char data_1 = 0;
unsigned char data_2 = 0;

unsigned char midi_msg = 0;
unsigned char midi_channel = 0;

unsigned int clk_counter = 0;

bool in_config_mode = 0;
absolute_time_t cfg_button_pressed_time = 0;

ChannelToPinMapping mapping[16] = {};

int main()
{
    setup();
    absolute_time_t current_time;

    while (true)
    {
        if (current_time % 1000 == 0)
        {
            printf("tick tock");
        }
        if (cfg_button_pressed_time > 0 && (current_time - cfg_button_pressed_time) > 1000)
        {
            set_config_mode(true);
            sleep_ms(10);
            cfg_button_pressed_time = 0;
        }

        if (in_config_mode)
        {
            midi_config();
        }
        else
        {
            run();
        }

        current_time = get_absolute_time();
        printf("time: %u\n", current_time);
        sleep_ms(1);
    }
    return 0;
}

void setup()
{
    stdio_init_all();

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    // UART setup
    uart_init(UART_ID, 2400);
    gpio_set_function(UART_RX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_RX_PIN));
    uart_set_baudrate(UART_ID, BAUD_RATE);
    uart_set_hw_flow(UART_ID, false, false);
    uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);
    uart_set_fifo_enabled(UART_ID, false);

    int UART_IRQ = UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;
    // TODO: irq_set_exclusive_handler disables the Button IRQ below
    // need to do multicore to have IRQ on both UART and GPIO
    // https://github.com/raspberrypi/pico-examples/blob/master/multicore/multicore_fifo_irqs/multicore_fifo_irqs.c
    irq_set_exclusive_handler(UART_IRQ, on_uart_rx);
    irq_set_enabled(UART_IRQ, true);
    uart_set_irqs_enabled(UART_ID, true, false);

    // Config button setup
    // FALL IS PRESS, RISE IS RELEASE
    gpio_set_irq_enabled(CFG_BUTTON_GPIO, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
    gpio_init(CFG_BUTTON_GPIO);
    gpio_set_dir(CFG_BUTTON_GPIO, GPIO_IN);
    gpio_pull_up(CFG_BUTTON_GPIO);
    gpio_set_irq_callback(&gpio_callback);

    for (int i = 0; i < 16; i++)
    {
        gpio_set_function(6 + i, GPIO_FUNC_PWM);
        uint slice_num = pwm_gpio_to_slice_num(i);
        pwm_set_phase_correct(slice_num, false);
        pwm_set_wrap(slice_num, 12000);
        pwm_set_enabled(slice_num, true);
    }

    sleep_ms(1000);
    printf("Started up!\n");
    blink_led(10, 20);
}

void midi_config()
{
    blink_led(3, LED_DELAY_MS);
}

void run()
{
    status = midi_rx();
    data_1 = midi_rx();
    data_2 = midi_rx();

    midi_msg = status & 0xF0;
    midi_channel = status & 0x0F;

    if ((status & MIDI_CLK) == MIDI_CLK)
    {
        clk_counter++;
        if (clk_counter >= 24)
        {
            printf("CLK, QUARTER NOTE\n");
            clk_counter = 0;
        }
    }

    switch (midi_msg)
    {
    case NOTE_ON:
        print_midi_msg(status, data_1, data_2);
        printf("NOTE ON: ");
        printf("Channel %u, ", midi_channel);
        printf("Note %u\n", data_1);
        break;
    case NOTE_OFF:
        print_midi_msg(status, data_1, data_2);
        printf("NOTE OFF: ");
        printf("Channel %u, ", midi_channel);
        printf("Note %u\n", data_1);

        // pass data along to the correct channel
        break;
    default:
        break;
    }
}

void gpio_callback(uint gpio, uint32_t events)
{
    if (gpio == CFG_BUTTON_GPIO && events & GPIO_IRQ_EDGE_FALL)
    {
        printf("gpio_callback%d PRESS\n", gpio);
        if (in_config_mode)
        {
            blink_led(2, LED_DELAY_MS);
            set_config_mode(false);
        }
        else
        {
            cfg_button_pressed_time = get_absolute_time();
        }
    }

    if (gpio == CFG_BUTTON_GPIO && events & GPIO_IRQ_EDGE_RISE)
    {
        printf("gpio_callback%d RELEASE\n", gpio);
        cfg_button_pressed_time = 0;
    }
}

void on_uart_rx()
{
    while (uart_is_readable(UART_ID))
    {
        uint8_t ch = uart_getc(UART_ID);
        printf("%x ", ch);
    }
}

unsigned char midi_rx()
{
    while (!uart_is_readable(UART_ID))
    {
    }
    unsigned char rx = uart_getc(UART_ID);

    return rx;
}

void set_config_mode(bool enabled)
{
    in_config_mode = enabled;
    if (in_config_mode)
    {
        printf("Entered config mode");
        blink_led(3, LED_DELAY_MS);
    }
    else
    {
        printf("Exited config mode");
        blink_led(2, LED_DELAY_MS);
    }
}

void print_midi_msg(char status, char data_1, char data_2)
{
    printf("full message");
    printf("0x%x ", status);
    printf("0x%x ", data_1);
    printf("0x%x\n", data_2);
}

void pico_set_led(bool led_on)
{
#if defined(PICO_DEFAULT_LED_PIN)
    // Just set the GPIO on or off
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
#elif defined(CYW43_WL_GPIO_LED_PIN)
    // Ask the wifi "driver" to set the GPIO on or off
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
#endif
}

void blink_led(int num_times, int delay_ms)
{
    for (int i = 0; i < num_times; i++)
    {
        pico_set_led(true);
        sleep_ms(delay_ms);
        pico_set_led(false);
        sleep_ms(delay_ms);
    }
}

void gate_out(int gpio, bool on)
{
}

void note_out(int gpio, int note)
{
    uint16_t pwm_value = note * 100;
    pwm_set_gpio_level(gpio, pwm_value);
}