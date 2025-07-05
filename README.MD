# Summary

This is a MIDI to CV converter written in C++ running on a raspberry pico.
It functions by reading MIDI messages through an optocoupler, and outputting multiple pitch and gate signals using PWM DAC.

# References

[MIDI spec](http://midi.teragonaudio.com/tech/midispec.htm)

[MIDI parsing](https://github.com/lennartquerter/euro-rack-synth/tree/main/modules/utility/1-midi-2-cv/firmware)

[PWM DAC on raspi](https://github.com/Aggebitter/RP2040_PWM2CV_test/blob/main/RP2040_PWM_test/RP2040_PWM_test.ino)
