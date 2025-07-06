#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "../inc/buffer.h"
#include "common.h"
#include "core1.h"

Core1 *Core1::global_instance = nullptr;

Buffer rx_buffer;
unsigned int clk_counter;
CV_output outputs[NUM_OUTPUTS];

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

    mock_output_config();

    printf("Mocked output pin GPIO: %d \n", outputs[0].gpio);

    // GPIO setup(outputs)
    for (size_t i = 0; i < NUM_OUTPUTS; i++)
    {
        setup_output_pin(outputs[i]);
    }

    global_instance = this;
}

void Core1::mock_output_config()
{
    for (size_t i = 0; i < NUM_OUTPUTS; i++)
    {
        outputs[i].channel = 7;
        outputs[i].gpio = FIRST_OUTPIT_PIN + i; // TODO: Offset pins when NUM_OUTPUTS > 1
        outputs[i].note = 0;
        outputs[i].gate_active = false;
    }
}

void Core1::setup_output_pin(CV_output conf)
{
    printf("Setting up output pin %u\n", conf.gpio);
    gpio_set_function(conf.gpio, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(conf.gpio);
    pwm_set_wrap(slice_num, 12000);
    pwm_set_enabled(slice_num, true);

    gpio_init(conf.get_gate_pin());
    gpio_set_dir(conf.get_gate_pin(), GPIO_OUT);
}

void Core1::run()
{
    uint8_t status;
    MIDI_event midi_event;
    MIDI_event_analysis analysis;

    while (1)
    {
        if (!buffer_is_empty(&rx_buffer))
        {
            uint8_t msg;
            if (buffer_pop(&rx_buffer, &msg) != BUFFER_SUCCESS)
                continue;

            unsigned char voice_category = 0;
            unsigned char channel = 0;

            switch (analysis.state)
            {
            case START_ANALYSIS:
                voice_category = msg & 0xF0;
                channel = msg & 0x0F;
                if ((msg & 0x80) == 0) // all types in MIDI_MSG_TYPE have 0x80 set to 1. This ignores the rest.
                    break;

                analysis.type = voice_category;
                analysis.channel = channel;
                analysis.state = WAIT_DATA_1;

                midi_event.channel = channel;
                midi_event.type = voice_category;
                break;
            case WAIT_DATA_1:
                if (analysis.type == PRGM_CHANGE || analysis.type == CHN_PRESSURE) // These types have a data length of 1 byte
                {
                    midi_event.data[0] = msg;
                    analysis.state = FINISHED_ANALYSIS;
                    finish_analysis(&analysis, &midi_event);
                }
                else // The rest have a data length of 2 bytes
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
    analysis->channel = 0;
    analysis->type = 0;

    midi_event->channel = 0;
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
    if (midi_event->channel > NUM_OUTPUTS - 1) // Error prevention until I can support 8 pwm outputs
        return;

    CV_output out = outputs[midi_event->channel];

    switch (midi_event->type)
    {
    case NOTE_ON:
        (&out)->gate_active = true;
        break;
    case NOTE_OFF:
        (&out)->gate_active = false;
        break;
    default:
        return;
        break;
    }

    if (out.note >= 0 && out.note <= 127)
    {
        (&out)->note = midi_event->data[0];
        common.print_midi_msg(midi_event->type | midi_event->channel, midi_event->data[0], midi_event->data[1]);

        output_cv(out);
    }
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

void Core1::output_cv(CV_output cv_out)
{
    printf("Outputting Note %d Gate %d on GPIO %d Exp %fV Amplified %fV\n", cv_out.note, cv_out.gate_active, cv_out.gpio, VOLT_PER_SEMITONE_OUT * (double)cv_out.note, (VOLT_PER_SEMITONE_OUT * (double)cv_out.note) * OPAMP_FACTOR);
    // TODO: Self-adjusting offset to get as close as possible to 1v/oct?
    pwm_set_gpio_level(cv_out.gpio, cv_out.note * 100);
    gpio_put(cv_out.get_gate_pin(), cv_out.gate_active);
}
