#include <stdio.h>
#include "pico/stdlib.h"

#define VOLT_MAX 3.3f // Maximum input voltage

#define LED_DELAY_MS 80
#define VOLT_PER_SEMITONE (1.0 / 12.0)
#define VOLT_PER_SEMITONE_OUT (3.3 / 120.0)
#define OPAMP_R_FEEDBACK_OHM 99600.0
#define OPAMP_R_GAIN_OHM 51000.0

// TODO: output should account for the gain factor not being exactly 3
#define OPAMP_GAIN_FACTOR 3 // (OPAMP_R_FEEDBACK_OHM / OPAMP_R_GAIN_OHM + 1) // = ~3V

// set this to determine sample rate
// 96     = 500,000 Hz
// 960   = 50,000 Hz
// 9600  = 5,000 Hz
#define FSAMP 5000 // Hz
#define CLOCK_DIV (48000000 / FSAMP)
#define ADC_CAPTURE_CHANNEL 0 // 26 is added to this sometimes don't worry about it :)

// BE CAREFUL: anything over about 9000 here will cause things
// to silently break. The code will compile and upload, but due
// to memory issues nothing will work properly
#define NSAMP 20

class Common
{
private:
    /* data */
public:
    const char *gpio_irq_str[4] = {
        "LEVEL_LOW",  // 0x1
        "LEVEL_HIGH", // 0x2
        "EDGE_FALL",  // 0x4
        "EDGE_RISE"   // 0x8
    };

    Common() {}
    ~Common() {}
    const void blink_led(int num_times, int delay_ms)
    {
        for (int i = 0; i < num_times; i++)
        {
            pico_set_led(true);
            sleep_ms(delay_ms);
            pico_set_led(false);
            sleep_ms(delay_ms);
        }
    }

    const void pico_set_led(bool led_on)
    {
#if defined(PICO_DEFAULT_LED_PIN)
        // Just set the GPIO on or off
        gpio_put(PICO_DEFAULT_LED_PIN, led_on);
#elif defined(CYW43_WL_GPIO_LED_PIN)
        // Ask the wifi "driver" to set the GPIO on or off
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
#endif
    }

    const void print_midi_msg(char type, char data_1, char data_2)
    {
        printf("MIDI event ");
        printf("0x%x ", type);
        printf("0x%x ", data_1);
        printf("0x%x\n", data_2);
    }

    const void gpio_event_string(char *buf, uint32_t events)
    {
        for (uint i = 0; i < 4; i++)
        {
            uint mask = (1 << i);
            if (events & mask)
            {
                // Copy this event string into the user string
                const char *event_str = gpio_irq_str[i];
                while (*event_str != '\0')
                {
                    *buf++ = *event_str++;
                }
                events &= ~mask;

                // If more events add ", "
                if (events)
                {
                    *buf++ = ',';
                    *buf++ = ' ';
                }
            }
        }
        *buf++ = '\0';
    }

    // Shifts the elements in a len(3) array one step to the left
    const void shift_array(uint8_t *arr)
    {
        arr[2] = arr[1];
        arr[1] = arr[0];
    }
};
