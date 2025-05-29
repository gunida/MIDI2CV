#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "../inc/buffer.h"
#include "common.h"
#include "core1.h"

#define UART_ID uart1
#define UART_RX_PIN 5
#define BAUD_RATE 31250
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY UART_PARITY_NONE
#define BUFFER_SIZE 128

#define NOTE_OFF 0x80     // 2 data bytes
#define NOTE_ON 0x90      // 2 data bytes
#define AFTERTOUCH 0xA0   // 2 data bytes
#define CTRL_CHANGE 0xB0  // 2 data bytes
#define PRGM_CHANGE 0xC0  // 1 data bytes
#define CHN_PRESSURE 0xD0 // 1 data bytes
#define WHEEL 0xE0        // 2 data bytes
#define MIDI_CLK 0xF8

Core1 *Core1::global_instance = nullptr;

Buffer rx_buffer;
unsigned int clk_counter;

Core1::Core1()
{
}

int Core1::main()
{
    printf("Core1 main\n");
    setup();
    run();
    return 0;
}

/// @brief Initializes UART, IRQ, and Buffer
void Core1::setup()
{
    clk_counter = 0;
    BUFFER_STATUS status = buffer_init(&rx_buffer, BUFFER_SIZE);
    if (status != BUFFER_SUCCESS)
    {
        printf("FATAL: Could not initialize Buffer.\n");
        throw;
    }

    // UART setup
    uart_init(UART_ID, 2400);
    gpio_set_function(UART_RX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_RX_PIN));
    uart_set_baudrate(UART_ID, BAUD_RATE);
    uart_set_hw_flow(UART_ID, false, false);
    uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);
    uart_set_fifo_enabled(UART_ID, false);

    // IRQ setup
    int UART_IRQ = UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;
    irq_set_exclusive_handler(UART_IRQ, rx_handler_ptr);
    irq_set_enabled(UART_IRQ, true);
    uart_set_irqs_enabled(UART_ID, true, false);

    global_instance = this;
}

void Core1::run()
{
    uint8_t status;

    while (1)
    {
        if (!buffer_is_empty(&rx_buffer) && buffer_get_size(&rx_buffer) > 1)
        {
            if (buffer_pop(&rx_buffer, &status) == BUFFER_SUCCESS)
                // printf("rx 0x%u\n", status);
                midi_msg_receiver(status);
        }
    }
}

/// @brief UART interrupt handler
void Core1::rx_handler()
{
    while (uart_is_readable(UART_ID))
    {
        uint8_t ch = uart_getc(UART_ID);
        if ((ch & MIDI_CLK) == MIDI_CLK)
        {
            uart_clk_handler();
            continue;
        }

        // Push the message into a buffer
        if (buffer_push(&rx_buffer, &ch) != BUFFER_SUCCESS)
        {
            printf("ERROR: Could not push to Buffer. Freeing it. \n");
            buffer_u8_free(&rx_buffer);
        }
    }
}

void Core1::midi_msg_receiver(uint8_t status)
{
    uint8_t midi_msg_buffer[3] = {};

    unsigned char voice_category = status & 0xF0;

    switch (voice_category)
    {
    case NOTE_ON:
    case NOTE_OFF:
        midi_msg_buffer[0] = status;
        buffer_pop(&rx_buffer, &midi_msg_buffer[1]);
        buffer_pop(&rx_buffer, &midi_msg_buffer[2]);

        midi_msg_handler(midi_msg_buffer[0], midi_msg_buffer[1], midi_msg_buffer[2]);
        break;
    case PRGM_CHANGE:
    case CHN_PRESSURE:
        buffer_pop(&rx_buffer, nullptr);
        break;
    case AFTERTOUCH:
    case CTRL_CHANGE:
    case WHEEL:
        buffer_pop(&rx_buffer, nullptr);
        buffer_pop(&rx_buffer, nullptr);
        break;
    default:
        printf("ERROR: %u is not a MIDI message type.\n", voice_category);
        break;
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
        printf("NOTE ON: Channel %u, Note %u\n", midi_channel, data_1);
        break;
    case NOTE_OFF:
        // print_midi_msg(status, data_1, data_2);
        printf("NOTE OFF: Channel %u, Note %u\n", midi_channel, data_1);

        // TODO: pass data along to the correct channel
        break;
    default:
        break;
    }
    // common.blink_led(1, LED_DELAY_MS);
}

/// @brief This method should send clock triggers on a GPIO pin
void Core1::uart_clk_handler()
{
    // TODO: Actual implementation
    clk_counter++;
    if (clk_counter >= 24)
    {
        printf("CLK, QUARTER NOTE\n");
        common.blink_led(1, LED_DELAY_MS);
        clk_counter = 0;
    }
}