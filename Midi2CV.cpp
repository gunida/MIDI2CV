#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/pwm.h"
#include "pico/multicore.h"

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

#define FLAG_VALUE1 123
#define FLAG_VALUE2 321

#define TIME_IN_MS_TO_ENTER_CONFIG 2000

#define DEBOUNCE_DELAY_TIME 5
unsigned long debounce_timer = to_ms_since_boot(get_absolute_time());

typedef struct
{
    short midi_channel;
    short gate_pin;
} ChannelToPinMapping;

enum State
{
    BOOT,
    INIT,
    PLAY,
    CONFIG,
    EXIT
};

void gpio_callback(uint gpio, uint32_t events);
void on_uart_rx();
void setup();
void midi_config(absolute_time_t prev_time, absolute_time_t current_time);
void config_btn_listener(absolute_time_t prev_time, absolute_time_t current_time);
void set_application_state(State state);
void run();
unsigned char midi_rx();
void print_midi_msg(char status, char data_1, char data_2);
void blink_led(int num_times, int delay_nms);
void gate_out(int gpio, bool on);
void note_out(int gpio, int note);

void gpio_event_string(char *buf, uint32_t events);

static char event_str[128];

static int core0_rx_val = 0, core1_rx_val = 0;

State application_state = BOOT;
unsigned char status = 0;
unsigned char data_1 = 0;
unsigned char data_2 = 0;

unsigned char midi_msg = 0;
unsigned char midi_channel = 0;

unsigned int clk_counter = 0;

bool in_config_mode = 0;
absolute_time_t cfg_button_pressed_time = 0;

ChannelToPinMapping mapping[16] = {};

// void core0_sio_irq()
// {
//     // Just record the latest entry
//     while (multicore_fifo_rvalid())
//         core0_rx_val = multicore_fifo_pop_blocking();

//     multicore_fifo_clear_irq();
// }

// void core1_sio_irq()
// {
//     // Just record the latest entry
//     while (multicore_fifo_rvalid())
//         core1_rx_val = multicore_fifo_pop_blocking();

//     multicore_fifo_clear_irq();
// }

// Checks for GPIO buttons, config
void core0_entry()
{

    sleep_ms(500);
    absolute_time_t current_time;
    absolute_time_t prev_time;
    while (1)
    {
        prev_time = current_time;
        current_time = to_ms_since_boot(get_absolute_time());

        switch (application_state)
        {
        case PLAY:
            config_btn_listener(prev_time, current_time);
            break;
        case CONFIG:
            midi_config(prev_time, current_time);
            break;
        case INIT:
        default:
            break;
        }

        if (prev_time != current_time && current_time % 1000 == 0)
            printf("in core0 loop\n");
    }
    printf("left the core0 loop\n");
}

// Checks for MIDI signals over UART
void core1_entry()
{
    /*
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

    // multicore_fifo_clear_irq();
    // irq_set_exclusive_handler(SIO_FIFO_IRQ_NUM(1), core1_sio_irq);
    // irq_set_enabled(SIO_FIFO_IRQ_NUM(1), true);

    // // Send something to Core0, this should fire the interrupt.
    // multicore_fifo_push_blocking(FLAG_VALUE1);
*/
    while (true)
    {
        sleep_ms(1000);
        // run();
        printf("core 1\n");
    }
}

int main()
{
    try
    {
        sleep_ms(500);
        setup();

        // multicore_launch_core1(core1_entry);
        set_application_state(PLAY);
        core0_entry();

        printf("Exiting main\n");
    }
    catch (...)
    {
        printf("Exception thrown\n");
    }

    return 0;
}

void setup()
{
    set_application_state(INIT);
    stdio_init_all();
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    // Config button setup
    // FALL IS PRESS, RISE IS RELEASE
    gpio_set_irq_enabled(CFG_BUTTON_GPIO, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
    gpio_init(CFG_BUTTON_GPIO);
    gpio_set_dir(CFG_BUTTON_GPIO, GPIO_IN);
    gpio_pull_up(CFG_BUTTON_GPIO);
    gpio_set_irq_callback(&gpio_callback);
    irq_set_enabled(IO_IRQ_BANK0, true);
    // END Config button setup

    // Output setup
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
    blink_led(10, 50);
}

void midi_config(absolute_time_t prev_time, absolute_time_t current_time)
{
    if (prev_time != current_time && current_time % 1000 == 0)
    {
        printf("In config mode\n ");
    }
    blink_led(1, LED_DELAY_MS);
}

void config_btn_listener(absolute_time_t prev_time, absolute_time_t current_time)
{
    if (cfg_button_pressed_time > 0 && (current_time - cfg_button_pressed_time) > TIME_IN_MS_TO_ENTER_CONFIG)
    {
        set_application_state(CONFIG);
        cfg_button_pressed_time = 0;
        return;
    }

    if (prev_time != current_time && current_time % 1000 == 0)
    {
        printf("core 0 Btn pressed: %llu, Current time: %llu\n ", cfg_button_pressed_time, current_time);
    }
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
    if ((to_ms_since_boot(get_absolute_time()) - debounce_timer) < DEBOUNCE_DELAY_TIME)
    {
        return;
    }

    debounce_timer = to_ms_since_boot(get_absolute_time());

    gpio_event_string(event_str, events);
    printf("GPIO %d %s\n", gpio, event_str);
    if (gpio == CFG_BUTTON_GPIO && events & GPIO_IRQ_EDGE_FALL)
    {
        if (cfg_button_pressed_time > 0)
            cfg_button_pressed_time = 0;
        // printf("gpio_callback%d PRESS\n", gpio);
        if (application_state == PLAY)
            cfg_button_pressed_time = to_ms_since_boot(get_absolute_time());
        if (application_state == CONFIG)
            set_application_state(PLAY);
    }
    else if (gpio == CFG_BUTTON_GPIO && events & GPIO_IRQ_EDGE_RISE)
    {
        // printf("gpio_callback%d RELEASE\n", gpio);
        cfg_button_pressed_time = 0;

        // if (application_state == CONFIG)
        // {
        //     set_application_state(PLAY);
        // }
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

void set_application_state(State state)
{
    printf("From State ");

    switch (application_state)
    {
    case BOOT:
        printf("BOOT");
        break;
    case INIT:
        printf("INIT");
        break;
    case PLAY:
        printf("PLAY");
        break;
    case CONFIG:
        printf("CONFIG");
        break;
    default:
        printf("__UNDEFINED__");
        break;
    }

    printf(" to ");

    application_state = state;

    switch (application_state)
    {
    case BOOT:
        printf("BOOT");
        break;
    case INIT:
        printf("INIT");
        break;
    case PLAY:
        printf("PLAY");
        break;
    case CONFIG:
        printf("CONFIG");
        break;
    default:
        printf("__UNDEFINED__");
        break;
    }

    printf("\n");

    sleep_ms(1);
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

static const char *gpio_irq_str[] = {
    "LEVEL_LOW",  // 0x1
    "LEVEL_HIGH", // 0x2
    "EDGE_FALL",  // 0x4
    "EDGE_RISE"   // 0x8
};

void gpio_event_string(char *buf, uint32_t events)
{
    for (uint i = 0; i < 4; i++)
    {
        uint mask = (1 << i);
        if (events & mask)
        {
            // Copy this event string into the user string
            const char *event_str = gpio_irq_str[i];
            while (*event_str != '\0')
            {
                *buf++ = *event_str++;
            }
            events &= ~mask;

            // If more events add ", "
            if (events)
            {
                *buf++ = ',';
                *buf++ = ' ';
            }
        }
    }
    *buf++ = '\0';
}