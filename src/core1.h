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
    static Core1 *global_instance;

    int main();
    void run();
    void setup();
    void rx_handler();
    int8_t read_uart_rx();
    void uart_clk_handler();
    void midi_msg_receiver(uint8_t status);
    void midi_msg_handler(uint8_t status, uint8_t note, uint8_t velocity);
    static void rx_handler_ptr()
    {
        if (global_instance)
            global_instance->rx_handler();
    }

public:
    Core1();
    void initialize()
    {
        main();
        // if (global_instance)
        //     global_instance->main();
    }
};
