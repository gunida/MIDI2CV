#include <stdio.h>
// #include <map>
#include "pico/stdlib.h"
#include "pico/time.h"
// #include "hardware/pwm.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
// #include "pico/multicore.h"
#include "../inc/buffer.h"
#include "common.h"
#include "core1.h"

#define UART_ID uart1
#define UART_RX_PIN 5
#define BAUD_RATE 31250
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY UART_PARITY_NONE

#define NOTE_ON 0x90
#define NOTE_OFF 0x80
#define MIDI_CLK 0xF8

Core1 *Core1::global_instance = nullptr;

Core1::Core1()
{
    setup();
}

void Core1::run()
{
    while (1)
    {
        prev_time = current_time;

        // TODO: Read midi messages from the buffer and act on them

        if (prev_time != current_time && current_time % 1000 == 0)
        {
            // printf("core 1 loop\n",
            //        cfg_button_pressed_time,
            //        current_time,
            //        state_to_string[application_state]);
        }
        sleep_ms(1);
        current_time = to_ms_since_boot(get_absolute_time());
    }
}

void Core1::rx_handler()
{
    if (uart_is_readable(UART_ID))
    {
        int8_t ch = uart_getc(UART_ID);
        bool is_status_msg = (ch & 0x80) == 0x80;
        bool is_status_clk = (ch & MIDI_CLK) == MIDI_CLK;

        if (is_status_clk)
        {
            uart_clk_handler();
            return;
        }
        else
        {
            // Push the message into a buffer
        }
    }
}

void Core1::setup()
{
    clk_counter = 0;
    global_instance = this;

    // UART setup
    uart_init(UART_ID, 2400);
    gpio_set_function(UART_RX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_RX_PIN));
    uart_set_baudrate(UART_ID, BAUD_RATE);
    uart_set_hw_flow(UART_ID, false, false);
    uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);
    // uart_set_fifo_enabled(UART_ID, false);

    int UART_IRQ = UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;
    // TODO: irq_set_exclusive_handler disables the Button IRQ below
    // need to do multicore to have IRQ on both UART and GPIO
    // https://github.com/raspberrypi/pico-examples/blob/master/multicore/multicore_fifo_irqs/multicore_fifo_irqs.c
    irq_set_exclusive_handler(UART_IRQ, trampolineHandler);
    irq_set_enabled(UART_IRQ, true);
    uart_set_irqs_enabled(UART_ID, true, false);
}

int8_t Core1::read_uart_rx()
{
    int8_t ch;
    if (!uart_is_readable_within_us(UART_ID, 350))
        return ch;

    while (uart_is_readable(UART_ID))
    {
        ch = uart_getc(UART_ID); // This should be the only call to uart_getc
        if ((ch & MIDI_CLK) == MIDI_CLK)
        {
            uart_clk_handler();
            continue;
        }
        break;
    }
    return ch;
}

void Core1::uart_clk_handler()
{
    clk_counter++;
    if (clk_counter >= 24)
    {
        printf("CLK, QUARTER NOTE\n");
        common.blink_led(1, LED_DELAY_MS);
        clk_counter = 0;
    }
}

void Core1::midi_msg_handler(uint8_t status, uint8_t data_1, uint8_t data_2)
{
    unsigned char voice_category = status & 0xF0;
    unsigned char midi_channel = status & 0x0F;

    common.print_midi_msg(status, data_1, data_2);

    switch (voice_category)
    {
    case NOTE_ON:
        // print_midi_msg(status, data_1, data_2);
        printf("NOTE ON: ");
        printf("Channel %u, ", midi_channel);
        printf("Note %u\n", data_1);
        break;
    case NOTE_OFF:
        // print_midi_msg(status, data_1, data_2);
        printf("NOTE OFF: ");
        printf("Channel %u, ", midi_channel);
        printf("Note %u\n", data_1);

        // TODO: pass data along to the correct channel
        break;
    default:
        break;
    }
    common.blink_led(1, 80);
}

// void on_uart_rx()
// {
//     uint8_t midi_msg_buffer[3] = {};

//     while (uart_is_readable(UART_ID))
//     {
//         int8_t ch = uart_getc(UART_ID);
//         bool is_status_msg = (ch & 0x80) == 0x80;

//         if (!is_status_msg)
//             break;

//         unsigned char voice_category = ch & 0xF0;

//         if ((ch & MIDI_CLK) == MIDI_CLK)
//         {
//             uart_clk_handler();
//             continue;
//         }

//         if ((voice_category & NOTE_ON) == NOTE_ON)
//         {
//             midi_msg_buffer[0] = ch;
//             midi_msg_buffer[1] = read_uart_rx();
//             midi_msg_buffer[2] = read_uart_rx();

//             midi_msg_handler(midi_msg_buffer[0], midi_msg_buffer[1], midi_msg_buffer[2]);
//             continue;
//         }
//         else if ((voice_category & NOTE_OFF) == NOTE_OFF)
//         {
//             midi_msg_buffer[0] = ch;
//             midi_msg_buffer[1] = read_uart_rx();
//             midi_msg_buffer[2] = read_uart_rx();

//             midi_msg_handler(midi_msg_buffer[0], midi_msg_buffer[1], midi_msg_buffer[2]);
//             continue;
//         }
//     }
// }