#include "../inc/buffer.h"

#define UART_ID uart1
#define UART_RX_PIN 5
#define BAUD_RATE 31250
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY UART_PARITY_NONE
#define BUFFER_SIZE 128

#define FIRST_OUTPIT_PIN 16
#define NUM_OUTPUTS 2

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
    /// @brief The current state of this analysis object
    MIDIMSG_STATE state;

    /// @brief This will be interpreted as a @see MIDI_MSG_TYPE
    unsigned char type;

    /// @brief MIDI Channel, 0-15
    uint8_t channel;
} MIDI_event_analysis;

typedef struct
{
    /// @brief this will be interpreted as a @see MIDI_MSG_TYPE
    unsigned char type() {
        //printf("MIDI MESSAGE TYPE: %d\r\n", data[0] & 0xF0);
        return data[0] & 0xF0;
    }

    /// @brief MIDI Channel, 0-15
    uint channel() {
        //printf("MIDI MESSAGE CHAN: %d\r\n", data[0] & 0x0F);
        return data[0] & 0x0F;
    }

    /// @brief MIDI messages are up to 4 bytes long
    uint8_t data[4];
} MIDI_event;

typedef struct
{
    /// @brief GPIO pin number for PWM output
    uint gpio_pwm;

    /// @brief GPIO pin number for Gate output
    uint gpio_gate;

    /// @brief MIDI Channel, 0-15
    uint channel;

    /// @brief MIDI Note number 0-120
    uint note;

    /// @brief Indicates if the output gate should be active or not
    bool gate_active;

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
    CV_output midi_msg_handler(MIDI_event *midi_event);
    void finish_analysis(MIDI_event_analysis *analysis, MIDI_event *midi_event);

    void setup_output_pin(CV_output conf);
    void output_cv(CV_output cv_out);
    void run_calibration(MIDI_event midi_event);
    int getOutputVoltageCorrection(float desired_voltage);
    void sample(uint8_t *capture_buf, int adc_channel);

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
