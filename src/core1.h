#include "../inc/buffer.h"

#define UART_ID uart1
#define UART_RX_PIN 5
#define BAUD_RATE 31250
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY UART_PARITY_NONE
#define BUFFER_SIZE 128

#define FIRST_OUTPIT_PIN 16
#define NUM_OUTPUTS 1

typedef enum
{
    NOTE_OFF = 0x80,     // 2 data bytes
    NOTE_ON = 0x90,      // 2 data bytes
    AFTERTOUCH = 0xA0,   // 2 data bytes
    CTRL_CHANGE = 0xB0,  // 2 data bytes
    PRGM_CHANGE = 0xC0,  // 1 data bytes
    CHN_PRESSURE = 0xD0, // 1 data bytes
    WHEEL = 0xE0,        // 2 data bytes
    MIDI_CLK = 0xF8
} MIDI_MSG_TYPE;

typedef enum
{
    START_ANALYSIS,
    WAIT_DATA_1,
    WAIT_DATA_2,
    FINISHED_ANALYSIS
} MIDIMSG_STATE;

typedef struct
{
    MIDIMSG_STATE state;
    unsigned char type;
    uint8_t channel;
} MIDI_event_analysis;

typedef struct
{
    unsigned char type;
    uint channel;
    uint8_t data[4];
} MIDI_event;

typedef struct
{
    uint gpio;
    uint channel;
    uint note;
    bool gate_active;

    uint get_gate_pin()
    {
        return gpio + NUM_OUTPUTS;
    }
} CV_output;

class Core1
{
private:
    Common common;
    static Core1 *global_instance;

    int main();
    void mock_output_config();
    void run();
    void setup();
    void rx_handler();
    int8_t read_uart_rx();
    void uart_clk_handler();
    void midi_msg_handler(MIDI_event *midi_event);
    void finish_analysis(MIDI_event_analysis *analysis, MIDI_event *midi_event);

    void setup_output_pin(CV_output conf);
    void output_cv(CV_output cv_out);

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
    }
};
