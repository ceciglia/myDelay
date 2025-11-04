# My Delay
## In brief
My delay produces classic chorus/flanger/vibrato effects?????? through real-time modulated fractional delay. It handles audio at 48Hz and 16 bit per sample to obtain an analogue-like sonic character/feeling.

## Technical overview and architecture
My delay is a DSP component written in C, designed to function as an audio element within the real-time audio pipeline of the ESP-ADF environment optimized for microcontroller execution.
It works at a sample rate of 48kHz, 16 bit per sample and it operates on fixed blocks of 512 bytes (BUF SIZE), therefore 256 samples (2 bytes per sample). All core DSP calculations (including LFO generation, interpolation and parameter smoothing) are performed using floating-point precision that is then recasted in int16_t. ????????All the project revolves arount the my_Delay struct which defines all the components necessary to the delay to function.??????????

As I mentioned before my delay is a member of the audio pipeline defined as follows:

```

[codec_chip] ---> i2s_stream_reader ---> delay ---> i2s_stream_writer ---> [codec_chip]

```
where i2s_stream_reader and i2s_stream_writer are audio elements responsible for acquiring of audio data and then sending the data out after processing. The i2s_stream_reader reads the audio data in input, the i2s_stream_writer writes it to the output.

### My delay design
The following diagram illustrates the architecture of the delay module, showing the relations between the DSP functional blocks.

![Diagramma a blocchi del modulo di Delay](images/mydelaylightandborder20.drawio.svg)


## DSP overview

### Fractional delay
In order to obtain a chorus/flanger effect the delay through precise control of the delay time. The delay line uses a circular buffer (delay_memory) and two heads: the write head (write_index) that writes into the buffer and the read head (read_index) that reads the buffer data after x delay time ($x \cdot T_{s}$ samples). Because the time modulation forces the read index to _continuously_ (i.e. fractionally) move between integer indexes, interpolation must be used to calculate the true audio value. The interpolation employed ??? is all pass interpolation, that ensures no frequency distortion (flat magnitude response) and it is defined as follows:
$$
y[n] = a_{1}(x[n] – y[n-1]) + x[n-1]
$$

### Modulation
The LFO Engine (LFO_t) provides the modulation source. It supports two waveforms (sine and square) and is responsible for producing a continuous, low-frequency signal between $0.01$ and $20$ Hz. The LFO's phase (current_phase) is managed with a wrap-around logic (from $0$ to $1$), ensuring the modulation cycle is perfectly smooth and continuous.

### Parameter smoothing and control
The project includes the implementation of parameter smoothing to prevent zipper noise (a rapid, audible stepping effect) when controls are adjusted. My delay addresses this by using a target-based structure for all critical parameters. Every parameter that can be changed by the user (e.g., base_dt, feedback, dw_ratio, LFO->frequency) has a corresponding **_target** field. The parameter value is gradually moved toward the target value over several samples. 

The smoothing function is a $I$ order low-pass filter (LPF): 

$$
y[i] = y[i-1] + alpha \cdot (x[i]-y[i-1])
$$

where the alpha coefficient is a hardcoded and pre-determined constant derived from empirical testing:
$$
alpha = 1 - e^{\frac{-1}{(0.3 \cdot F_{s})}};
$$


## Environment configuration
The project has been developed for the ESP32-A1S audio kit on VS Code (Visual Studio Code) using ESP-IDF and the ESP-ADF extention. The configuration has been completed following these [guide lines](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/) on the ESPRESSIF website.

### ESP-ADF
Espressif Systems Advanced Development Framework (ESP-ADF) is the official Advanced Development Framework for the ESP32, ESP32-S2, ESP32-C3, ESP32-C6, ESP32-S3, and ESP32-P4 SoCs. 

### ESP32-A1S Audio Kit
The project runs on the `ESP32-A1S Audio Kit v2.2 A436` development board. The board setup files used are available in this [repository](https://github.com/trombik/esp-adf-component-ai-thinker-esp32-a1s?tab=readme-ov-file).

### Codec setup 
The latest versions of the ESP32-A1S Audio Kit has the ES8388 codec on board. In order to meet the specifications of the delay design some registers default values has been changed. Specifically the es8388.c file at this path (C:\Users\cecix\esp\esp-adf\components\audio_hal\driver\es8388\es8388.c) has been modified: 
```c
esp_err_t es8388_init(audio_hal_codec_config_t *cfg)
{
    // other code
    
    // res |= es_write_reg(ES8388_ADDR, ES8388_DACCONTROL16, 0x09);  // 0x00 audio on LIN1&RIN1,  0x09 LIN2&RIN2
    // res |= es_write_reg(ES8388_ADDR, ES8388_DACCONTROL7, 0x00);  

    // other code

    // set also L2 and R2 volume to 0dB (0x1E) to hear from the headphones output
    res |= es_write_reg(ES8388_ADDR, ES8388_DACCONTROL26, 0x1E); 
    res |= es_write_reg(ES8388_ADDR, ES8388_DACCONTROL27, 0x1E);

    // other code

    // // set differential input select to LINPUT2-RINPUT2
    // res |= es_write_reg(ES8388_ADDR, ES8388_ADCCONTROL3, 0x82);

    // // set new min (-12dB) and max (35.5dB) PGA gain
    // res |= es_write_reg(ES8388_ADDR, ES8388_ADCCONTROL10, 0x38);

    // 16 Bits length, I2S serial audio data format and I2S channels swap by adjusting the L/R polarity settings in the codec driver
    res |= es_write_reg(ES8388_ADDR, ES8388_ADCCONTROL4, 0x2c);

    // // set Master mode ADC MCLK to sampling frequency ratio to 128 
    // res |= es_write_reg(ES8388_ADDR, ES8388_ADCCONTROL5, 0x00); 

    // set mic left and right channel PGA gain to 0dB (it was set to 0xbb, above +24dB)
    res |= es_write_reg(ES8388_ADDR, ES8388_ADCCONTROL1, 0x00);  // MIC Left and Right channel PGA gain 

    // other code
}
```


## How to Use the Example

### Example Functionality
```c
ciao
```


### Example Log
A complete log is as follows:

```c
ciao

```

## Troubleshooting

If there is no sound from the development board, or the sound is very weak, this is because by default the volume of the sound input from`AUX_IN` is low. So please increase the volume on the `Line-Out` end so that you can hear the audio output from the board.


## Technical Support and Feedback
Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
