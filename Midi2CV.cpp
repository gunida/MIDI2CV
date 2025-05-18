#include <stdio.h>
#include "pico/stdlib.h"

#define UART_ID uart1
#define UART_RX_PIN 5
#define BAUD_RATE 31250

unsigned char midi_rx();

int main()
{
    unsigned char midichar = 0;

    stdio_init_all();

    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    uart_set_fifo_enabled(UART_ID, true);

    sleep_ms(5000);
    printf("Started up!\n");

    while (true)
    {
        midichar = midi_rx();
        printf("%d\n", midichar);
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
