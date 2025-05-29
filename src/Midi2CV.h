
enum State
{
    BOOT,
    INIT,
    PLAY,
    CONFIG,
    EXIT
};

typedef struct
{
    short midi_channel;
    short gate_pin;
} ChannelToPinMapping;

void gpio_callback(uint gpio, uint32_t events);

void core0_setup();
void midi_config_mode(absolute_time_t prev_time, absolute_time_t current_time);
void btn_cfg_listener(absolute_time_t prev_time, absolute_time_t current_time);
void set_application_state(State state);
void midi_msg_handler(uint8_t status, uint8_t data_1, uint8_t data_2);
unsigned char midi_rx();
// const void print_midi_msg(char status, char data_1, char data_2);
// void shift_array(uint8_t *arr);
// void pico_set_led(bool led_on);
// void blink_led(int num_times, int delay_nms);
void gate_out(int gpio, bool on);
void note_out(int gpio, int note);
