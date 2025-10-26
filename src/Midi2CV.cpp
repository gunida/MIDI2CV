

#include <stdio.h>
#include <map>
#include "hardware/pwm.h"
#include "pico/multicore.h"
#include "common.h"
#include "Midi2CV.h"
#include "core1.h"

#define CFG_BUTTON_GPIO 0

#define TIME_IN_MS_TO_ENTER_CONFIG 2000

#define DEBOUNCE_DELAY_TIME 5
unsigned long debounce_timer = to_ms_since_boot(get_absolute_time());

static std::map<State, const char *> state_to_string = {
    {BOOT, "BOOT"},
    {INIT, "INIT"},
    {PLAY, "PLAY"},
    {CONFIG, "CONFIG"},
    {EXIT, "EXITING"}};

static char event_str[128];

State application_state = BOOT;

bool in_config_mode = 0;
absolute_time_t cfg_button_pressed_time = at_the_end_of_time;

ChannelToPinMapping mapping[16] = {};

Common common;

// Checks for GPIO buttons, config
void core0_entry()
{
    absolute_time_t current_time;
    absolute_time_t prev_time;
    while (1)
    {
        prev_time = current_time;

        switch (application_state)
        {
        case BOOT:
        case INIT:
            common.pico_set_led(true);
            break;
        case PLAY:
            btn_cfg_listener(prev_time, current_time);
            break;
        case CONFIG:
            midi_config_mode(prev_time, current_time);
            break;
        default:
            printf("In an invalid state\n");
            break;
        }

        if (prev_time != current_time && current_time % 1000 == 0)
        {
            printf("core 0 loop - Btn pressed: %llu, Current time: %llu, State: %s\n ",
                   cfg_button_pressed_time,
                   current_time,
                   state_to_string[application_state]);
        }
        current_time = to_ms_since_boot(get_absolute_time());
    }
    printf("left the core0 loop\n");
}

void core1_entry()
{
    Core1 core1;
    core1.initialize();
}

int main()
{
    try
    {
        sleep_ms(500);
        core0_setup();
        multicore_launch_core1(core1_entry);

        set_application_state(PLAY);
        core0_entry();

        printf("Exiting main\n");
    }
    catch (...)
    {
        printf("Exception thrown\n");
    }

    return 0;
}

void core0_setup()
{
    set_application_state(INIT);
    stdio_init_all();
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    // Config button setup
    // FALL IS PRESS, RISE IS RELEASE
    gpio_set_irq_enabled(CFG_BUTTON_GPIO, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
    gpio_init(CFG_BUTTON_GPIO);
    gpio_set_dir(CFG_BUTTON_GPIO, GPIO_IN);
    gpio_pull_up(CFG_BUTTON_GPIO);
    gpio_set_irq_callback(&gpio_callback);
    irq_set_enabled(IO_IRQ_BANK0, true);
    // END Config button setup

    sleep_ms(1000);
    printf("Started up!\n");
    common.blink_led(10, 50);
}

void midi_config_mode(absolute_time_t prev_time, absolute_time_t current_time)
{
    if (!is_at_the_end_of_time(cfg_button_pressed_time))
        set_application_state(PLAY);
    common.blink_led(1, 250);
}

void btn_cfg_listener(absolute_time_t prev_time, absolute_time_t current_time)
{
    if (is_at_the_end_of_time(cfg_button_pressed_time))
        return;

    if ((current_time - cfg_button_pressed_time) > TIME_IN_MS_TO_ENTER_CONFIG)
    {
        if (application_state == PLAY)
            set_application_state(CONFIG);

        cfg_button_pressed_time = at_the_end_of_time;
    }
}

void gpio_callback(uint gpio, uint32_t events)
{
    if ((to_ms_since_boot(get_absolute_time()) - debounce_timer) < DEBOUNCE_DELAY_TIME)
    {
        return;
    }

    debounce_timer = to_ms_since_boot(get_absolute_time());

    common.gpio_event_string(event_str, events);
    printf("GPIO %d %s\n", gpio, event_str);
    if (gpio == CFG_BUTTON_GPIO && events & GPIO_IRQ_EDGE_FALL)
    {
        // // printf("gpio_callback%d PRESS\n", gpio);
        // if (application_state == PLAY)
        cfg_button_pressed_time = to_ms_since_boot(get_absolute_time());
        // if (application_state == CONFIG)
        //     set_application_state(PLAY);
    }
    else if (gpio == CFG_BUTTON_GPIO && events & GPIO_IRQ_EDGE_RISE)
    {
        // printf("gpio_callback%d RELEASE\n", gpio);
        cfg_button_pressed_time = at_the_end_of_time;

        // if (application_state == CONFIG)
        // {
        //     set_application_state(PLAY);
        // }
    }
}

void set_application_state(State state)
{
    printf("From State ");
    printf(state_to_string[application_state]);
    printf(" to ");

    application_state = state;

    printf(state_to_string[application_state]);
    printf("\n");

    sleep_ms(1);
}

void gate_out(int gpio, bool on)
{
}

void note_out(int gpio, int note)
{
    uint16_t pwm_value = note * 100;
    pwm_set_gpio_level(gpio, pwm_value);
}
