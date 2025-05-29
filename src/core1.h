// #include <stdio.h>
// #include <map>
// #include "pico/stdlib.h"
// #include "pico/time.h"
// #include "hardware/pwm.h"
// #include "hardware/irq.h"
// #include "hardware/uart.h"
// #include "pico/multicore.h"
#include "../inc/buffer.h"

#define UART_ID uart1
#define UART_RX_PIN 5
#define BAUD_RATE 31250
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY UART_PARITY_NONE

#define NOTE_ON 0x90
#define NOTE_OFF 0x80
#define MIDI_CLK 0xF8

class Core1
{
private:
    Common common;
    absolute_time_t current_time;
    absolute_time_t prev_time;

    Buffer rx_buffer;
    unsigned int clk_counter;
    static Core1 *global_instance;

    void setup();
    int8_t read_uart_rx();
    void uart_clk_handler();
    void midi_msg_handler(uint8_t status, uint8_t data_1, uint8_t data_2);
    static void trampolineHandler()
    {
        if (global_instance)
            global_instance->rx_handler();
    }

public:
    Core1();

    void run();
    void rx_handler();
};
