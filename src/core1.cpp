#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "../inc/buffer.h"
#include "common.h"
#include "core1.h"

Core1 *Core1::global_instance = nullptr;

Buffer rx_buffer;
unsigned int clk_counter;
MIDI_event_analysis analysis;

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

    // small sanity test
    for (uint8_t i = 0; i < 3; i++)
    {
        buffer_push(&rx_buffer, &i);
    }
    for (uint8_t i = 0; i < 3; i++)
    {
        uint8_t value;
        buffer_pop(&rx_buffer, &value);
        printf("%u", value);
    }
    printf("\n");
    printf("expected: 012\n");
    printf("buffer size: %u, exp 0\n", buffer_get_size(&rx_buffer));

    MIDI_event midi_event;

    while (1)
    {
        if (!buffer_is_empty(&rx_buffer))
        {
            uint8_t msg;
            if (buffer_pop(&rx_buffer, &msg) != BUFFER_SUCCESS)
                continue;

            unsigned char voice_category = 0;
            unsigned char channel = 16;

            // printf("msg: 0x%x\n", msg);

            switch (analysis.state)
            {
            case START_ANALYSIS:
                voice_category = msg & 0xF0;
                channel = msg & 0x0F;
                // printf("msg: 0x%x, type: 0x%x, channel: 0x%x, %x\n", msg, voice_category, channel, msg & 0x80);
                if ((msg & 0x80) == 0)
                    break;

                analysis.type = voice_category;
                analysis.channel = channel;
                analysis.state = WAIT_DATA_1;

                midi_event.channel = channel;
                midi_event.type = voice_category;
                break;
            case WAIT_DATA_1:
                if (analysis.type == PRGM_CHANGE || analysis.type == CHN_PRESSURE)
                {
                    midi_event.data[0] = msg;
                    analysis.state = FINISHED_ANALYSIS;
                    finish_analysis(&analysis, &midi_event);
                }
                else
                {
                    midi_event.data[0] = msg;
                    analysis.state = WAIT_DATA_2;
                    break;
                }
                break;
            case WAIT_DATA_2:
                midi_event.data[1] = msg;
                analysis.state = FINISHED_ANALYSIS;
                finish_analysis(&analysis, &midi_event);
                break;
            default:
                break;
            }
        }
    }
}

void Core1::finish_analysis(MIDI_event_analysis *analysis, MIDI_event *midi_event)
{
    midi_msg_handler(midi_event);
    analysis->state = START_ANALYSIS;
    analysis->channel = 16;
    analysis->type = 0;

    midi_event->channel = 16;
    midi_event->type = 0;
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

/// @brief Handles note on/off events
/// @param status full 2-byte message 0x<message type><midi channel>
void Core1::midi_msg_handler(MIDI_event *midi_event)
{
    common.print_midi_msg(midi_event->type | midi_event->channel, midi_event->data[0], midi_event->data[1]);

    switch (midi_event->type)
    {
    case NOTE_ON:
        // print_midi_msg(status, data_1, data_2);
        // printf("NOTE ON: Channel %u, Note %u\n", midi_event->channel, midi_event->data[0]);
        break;
    case NOTE_OFF:
        // print_midi_msg(status, data_1, data_2);
        // printf("NOTE OFF: Channel %u, Note %u\n", midi_event->channel, midi_event->data[0]);

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