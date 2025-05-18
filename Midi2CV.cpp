#include <stdio.h>
#include "pico/stdlib.h"

#define UART_ID uart1
#define UART_RX_PIN 5
#define BAUD_RATE 31250

#define NOTE_ON 0x90
#define NOTE_OFF 0x80
#define MIDI_CLK 0xF8

unsigned char midi_rx();
int print_midi_msg(char status, char data_1, char data_2);

int main()
{
    unsigned char status = 0;
    unsigned char data_1 = 0;
    unsigned char data_2 = 0;

    unsigned char midi_msg = 0;
    unsigned char midi_channel = 0;

    unsigned int clk_counter = 0;

    stdio_init_all();

    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    uart_set_fifo_enabled(UART_ID, true);

    sleep_ms(3000);
    printf("Started up!\n");

    while (true)
    {
        status = midi_rx();
        data_1 = midi_rx();
        data_2 = midi_rx();

        midi_msg = status & 0xF0;
        midi_channel = status & 0x0F;

        // print_midi_msg(status, data_1, data_2);

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
            break;
        default:
            break;
        }
    }
    return 0;
}

unsigned char midi_rx()
{
    while (!uart_is_readable(UART_ID))
    {
    }
    unsigned char rx = uart_getc(UART_ID);

    return rx;
}

int print_midi_msg(char status, char data_1, char data_2)
{
    printf("full message");
    printf("0x%x ", status);
    printf("0x%x ", data_1);
    printf("0x%x\n", data_2);
    return 0;
}
