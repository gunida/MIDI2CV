# Summary

This is a MIDI to CV converter written in C++ running on a raspberry pico.
It functions by reading MIDI messages through an optocoupler, and outputting multiple pitch and gate signals using PWM DAC.

# References

[MIDI spec](http://midi.teragonaudio.com/tech/midispec.htm)

[MIDI parsing](https://github.com/lennartquerter/euro-rack-synth/tree/main/modules/utility/1-midi-2-cv/firmware)

[PWM DAC on raspi](https://github.com/Aggebitter/RP2040_PWM2CV_test/blob/main/RP2040_PWM_test/RP2040_PWM_test.ino)


# Circuit

## DAC

Pico PWM runs at 10.43 kHz when setting the wrap of the PWM to 12000:\
`pwm_set_wrap(slice_num, 12000);`

Cutoff for the filter is 5 kHz using the following values

```
20 kOhm -+- 200 kOhm -+- Op Amp
         |            |
        15n          1n5
         |            |
        GND          GND

```

The signal is then amplified ~3x by the Op Amp configured as a noninverting amplifier.

## Optocoupler
MIDI through optocoupler into Pico\
Under construction...