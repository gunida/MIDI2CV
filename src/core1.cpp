#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "../inc/buffer.h"
#include "common.h"
#include "core1.h"

#define TEST_MODE 1

Core1 *Core1::global_instance = nullptr;

Buffer rx_buffer;
unsigned int clk_counter;
CV_output outputs[NUM_OUTPUTS];

dma_channel_config cfg;
uint dma_chan;
uint8_t cap_buf[NSAMP];
const float conversion_factor = VOLT_MAX / (1 << 8); // 256 bit, for DMA ADC conversion

int output_voltage_correction = 0;

Core1::Core1()
{
}

int Core1::main()
{
    printf("Core1 main\n");
    setup();
    common.blink_led(10, 50);
    sleep_ms(2000);
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

#pragma region UART setup
    uart_init(UART_ID, 2400);
    gpio_set_function(UART_RX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_RX_PIN));
    uart_set_baudrate(UART_ID, BAUD_RATE);
    uart_set_hw_flow(UART_ID, false, false);
    uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);
    uart_set_fifo_enabled(UART_ID, false);
#pragma endregion

#pragma region IRQ setup
    int UART_IRQ = UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;
    irq_set_exclusive_handler(UART_IRQ, rx_handler_ptr);
    irq_set_enabled(UART_IRQ, true);
    uart_set_irqs_enabled(UART_ID, true, false);
#pragma endregion

#pragma region ADC setup
    adc_gpio_init(ADC_CAPTURE_CHANNEL + 26);
    adc_init();
    adc_fifo_setup(
        true,  // Write each completed conversion to the sample FIFO
        true,  // Enable DMA data request (DREQ)
        1,     // DREQ (and IRQ) asserted when at least 1 sample present
        false, // We won't see the ERR bit because of 8 bit reads; disable.
        true   // Shift each sample to 8 bits when pushing to FIFO
    );

    // set ADC sample rate
    adc_set_clkdiv(CLOCK_DIV);

    // Set up the DMA to start transferring data as soon as it appears in FIFO
    uint dma_chan = dma_claim_unused_channel(true);
    cfg = dma_channel_get_default_config(dma_chan);

    // Reading from constant address, writing to incrementing byte addresses
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);

    // Pace transfers based on availability of ADC samples
    channel_config_set_dreq(&cfg, DREQ_ADC);
#pragma endregion

    // TODO: Configure outputs properly
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
    pwm_set_wrap(slice_num, 12000); // Pico PWM runs at 10.43 kHz when setting the wrap of the PWM to 12000
    pwm_set_enabled(slice_num, true);

    gpio_init(conf.get_gate_pin());
    gpio_set_dir(conf.get_gate_pin(), GPIO_OUT);
}

void Core1::run()
{
    uint8_t status;
    MIDI_event midi_event;
    MIDI_event_analysis analysis;

#if TEST_MODE

    while (1)
    {

        run_calibration(midi_event);

        for (size_t i = 0; i < 12; i++)
        {
            midi_event.channel = 0; // this is the default
            midi_event.type = NOTE_ON;

            midi_event.data[0] = i + 12 + 36;
            midi_event.data[1] = 0x40;
            midi_msg_handler(&midi_event);

            common.blink_led(1, 2000);
            midi_event.type = NOTE_OFF;
            midi_msg_handler(&midi_event);
            common.blink_led(1, 250);
            common.blink_led(3, 50);
        }
    }

#endif

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

/// @brief Checks offset between octave 0 and 8 a couple of times 
/// @param midi_event 
void Core1::run_calibration(MIDI_event midi_event)
{
    int sum_correction = 0;
    const int num_passes = 8;
    for (size_t i = 0; i < num_passes / 2; i++)
    {
        for (int j = 0; j < 10; j+= 8)
        {
            midi_event.channel = 0; // this is the default
            midi_event.type = NOTE_ON;

            midi_event.data[0] = j * 12;
            midi_event.data[1] = 0x40;
            CV_output output = midi_msg_handler(&midi_event);

            float exp_voltage = VOLT_PER_SEMITONE_OUT * (double)output.note;
            int correction = getOutputVoltageCorrection(exp_voltage);
            output_voltage_correction = correction;
            sum_correction += correction;

            common.blink_led(1, 1000);
            // midi_event.type = NOTE_OFF;
            // midi_msg_handler(&midi_event);
            common.blink_led(3, 50);
        }
    }
    float avgCorrection = sum_correction / num_passes;
    output_voltage_correction = avgCorrection;

    printf("Averaged corrections to %d\n", output_voltage_correction);
}

#pragma region MIDI messaging
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
CV_output Core1::midi_msg_handler(MIDI_event *midi_event)
{
    CV_output out;
    if (midi_event->channel > NUM_OUTPUTS - 1) // Error prevention until I can support 8 pwm outputs
        return out;

    out = outputs[midi_event->channel];

    switch (midi_event->type)
    {
    case NOTE_ON:
        (&out)->gate_active = true;
        break;
    case NOTE_OFF:
        (&out)->gate_active = false;
        break;
    default:
        return out;
        break;
    }

    if (out.note >= 0 && out.note <= 127)
    {

        (&out)->note = midi_event->data[0];
        common.print_midi_msg(midi_event->type | midi_event->channel, midi_event->data[0], midi_event->data[1]);

        output_cv(out);
    }
    return out;
}

/// @brief This method should send clock triggers on a GPIO pin
void Core1::uart_clk_handler()
{
    // TODO: Actual implementation
    clk_counter++;
    if (clk_counter >= 24)
    {
        printf("CLK, QUARTER NOTE\n");
        // common.blink_led(1, LED_DELAY_MS);
        clk_counter = 0;
    }
}

void Core1::output_cv(CV_output cv_out)
{
    float exp_voltage = VOLT_PER_SEMITONE_OUT * (double)cv_out.note;
    float exp_amped_voltage = (VOLT_PER_SEMITONE_OUT * (double)cv_out.note) * OPAMP_GAIN_FACTOR;
    if (cv_out.gate_active)
        printf("Outputting Note %d Gate %d on GPIO %d Exp %fV Amplified %fV\n", cv_out.note, cv_out.gate_active, cv_out.gpio, exp_voltage, exp_amped_voltage);

    pwm_set_gpio_level(cv_out.gpio, cv_out.note * 100 + output_voltage_correction);
    getOutputVoltageCorrection(exp_voltage);

    // TODO: A setting for retrigger.
    // Change the voltage regardless of gate position, but keep the gate open as long as any gate is open

    gpio_put(cv_out.get_gate_pin(), cv_out.gate_active);
}
#pragma endregion

#pragma region Output voltage correction

void Core1::sample(uint8_t *capture_buf, int adc_channel)
{
    adc_select_input(adc_channel);

    adc_fifo_drain();
    adc_run(false);

    dma_channel_configure(dma_chan, &cfg,
                          capture_buf,   // dst
                          &adc_hw->fifo, // src
                          NSAMP,         // transfer count
                          true           // start immediately
    );

    adc_run(true);
    dma_channel_wait_for_finish_blocking(dma_chan);
}

int Core1::getOutputVoltageCorrection(float desired_voltage)
{
    sleep_ms(10); // sleep a little to let the CV stabilize
    sample(cap_buf, ADC_CAPTURE_CHANNEL);
    uint64_t sum = 0;
    for (int i = 0; i < NSAMP; i++)
    {
        sum += cap_buf[i];
    }
    float avg = (float)sum / NSAMP;
    float actual_voltage = avg * conversion_factor;

    float diff = (desired_voltage - actual_voltage) / VOLT_PER_SEMITONE_OUT;

    printf("Measured voltage: %f, desired voltage: %f, percentual diff: %f\n", actual_voltage, desired_voltage, diff);
    return diff;
}

#pragma endregion