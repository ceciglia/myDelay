# My Delay
## In brief
My delay produces classic chorus/flanger/vibrato effects through real-time modulated fractional delay. It handles audio at 48Hz and 16 bit per sample to achieve an analogue-like sonic character. The ultimate goal of the development is integration into a dedicated guitar pedal product.

## Technical overview and architecture
My delay is a DSP component written in C, designed to function as an audio element within the real-time audio pipeline of the ESP-ADF environment, optimized for microcontroller execution.
It works at a sample rate of 48kHz, 16 bit per sample, 2 channels. The audio element operates on fixed blocks of bytes (BUF SIZE = 512 in this case, therefore 256 samples, i.e. 2 bytes per sample). All core DSP calculations (including LFO generation, interpolation and parameter smoothing) are performed using floating-point precision that is then recasted in _int16_t_. 

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
In order to obtain a chorus/flanger/vibrato effect the delay operates through precise control of the delay time. The delay line uses a circular buffer (delay_memory) and two heads: the write head (write_index) that writes into the buffer and the read head (read_index) that reads the buffer data after $x$ delay time ($x \cdot T_{s}$ samples). Because the time modulation forces the read index to _continuously_ (i.e. fractionally) move between integer indexes, interpolation must be used to calculate the true audio value. The interpolation employed is all-pass interpolation, that ensures no frequency distortion and flat magnitude response:
$$
y[n] = alpha \cdot (x[n] – y[n-1]) + x[n-1]
$$
The module also includes a feedback mechanism, where a portion of the delayed signal is fed back and mixed with the input of the delay. This parameter is crucial for enhancing the metallic resonance of the flanger effect.

### Modulation
The LFO Engine (LFO_t) provides the modulation source. It supports two waveforms (sine and square) and is responsible for producing a continuous, low-frequency signal operating in the range of $0.01$ to $20$ Hz. The LFO's phase accumulator (current_phase) is managed with a wrap-around logic (from $0$ to $1$) preventing phase discontinuities. The modulation depth is set via the LFO->mod_amount parameter. This value directly correlates the maximum deviation of the delay time, specified in seconds.

### Parameter smoothing and control
The project includes the implementation of parameter smoothing to prevent zipper noise (a rapid, audible stepping effect) when controls are adjusted. My delay addresses this by using a target-based structure for all critical parameters. Every parameter that can be changed by the user (e.g., base_dt, feedback, dw_ratio, LFO->frequency) has a corresponding **_target** field. The parameter value is gradually moved toward the target value over several samples. 

The smoothing function is a $I$ order low-pass filter (LPF): 

$$
y[n] = y[n-1] + alpha \cdot (x[n]-y[n-1])
$$

where the alpha coefficient is a hardcoded and pre-determined constant derived from empirical testing:
$$
alpha = 1 - e^{\frac{-1}{(0.3 \cdot F_{s})}};
$$
(N.B. the $alpha$ defined here is different from the one used in the all-pass interpolation)


## Environment configuration
The project has been developed for the ESP32-A1S audio kit on VS Code (Visual Studio Code) using ESP-IDF extention and ESP-ADF framework. The configuration has been completed following these [guide lines](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/) on the ESPRESSIF website.

### ESP-ADF
Espressif Systems Advanced Development Framework (ESP-ADF) is the official Advanced Development Framework for the ESP32, ESP32-S2, ESP32-C3, ESP32-C6, ESP32-S3, and ESP32-P4 SoCs. For this project, I specifically used the framework's core Audio Development Framework (ADF) component, which provides the necessary functions for real-time audio processing and control.

### ESP32-A1S Audio Kit
The project runs on the `ESP32-A1S Audio Kit v2.2 A436` development board. The board setup files used are available in this [repository](https://github.com/trombik/esp-adf-component-ai-thinker-esp32-a1s?tab=readme-ov-file). I've made the following changes in `boards_pins_config.c`:
```c
esp_err_t get_i2s_pins(int port, board_i2s_pin_t *i2s_config)    // here
{
    AUDIO_NULL_CHECK(TAG, i2s_config, return ESP_FAIL);
    if (port == 0) {                                             // here
        i2s_config->mck_io_num = GPIO_NUM_0;
#if defined(CONFIG_AI_THINKER_ESP32_A1S_ES8388_VARIANT_7)
        i2s_config->bck_io_num = GPIO_NUM_5;
#elif defined(CONFIG_AI_THINKER_ESP32_A1S_ES8388_VARIANT_5)
        i2s_config->bck_io_num = GPIO_NUM_27;
#endif
        i2s_config->ws_io_num = GPIO_NUM_25;
        i2s_config->data_out_num = GPIO_NUM_26;
        i2s_config->data_in_num = GPIO_NUM_35;
    } else if (port == 1) {                                      // here
        i2s_config->bck_io_num = -1;
        i2s_config->ws_io_num = -1;
        i2s_config->data_out_num = -1;
        i2s_config->data_in_num = -1;
    } else {
        memset(i2s_config, -1, sizeof(board_i2s_pin_t));
        ESP_LOGE(TAG, "i2s port %d is not supported", port);
        return ESP_FAIL;
    }

    return ESP_OK;
}
```
And also in `board_def.h` in order to select a different ADC input (AUDIO_HAL_ADC_INPUT_LINE2 in this case):
```c
#define AUDIO_CODEC_DEFAULT_CONFIG(){                   \
        .adc_input  = AUDIO_HAL_ADC_INPUT_LINE2,        \   // here
        .dac_output = AUDIO_HAL_DAC_OUTPUT_ALL,         \
        .codec_mode = AUDIO_HAL_CODEC_MODE_BOTH,        \
        .i2s_iface = {                                  \
            .mode = AUDIO_HAL_MODE_SLAVE,               \
            .fmt = AUDIO_HAL_I2S_NORMAL,                \
            .samples = AUDIO_HAL_48K_SAMPLES,           \
            .bits = AUDIO_HAL_BIT_LENGTH_16BITS,        \
        },                                              \
};
```

### Codec setup 
The latest versions of the ESP32-A1S Audio Kit has the ES8388 codec on board. In order to meet the specifications of the delay design some registers default values has been changed. Specifically the es8388.c file at this path: "<ADF_PATH>\components\audio_hal\driver\es8388\es8388.c" has been modified: 
```c
esp_err_t es8388_init(audio_hal_codec_config_t *cfg)
{
    // other code

    // set also L2 and R2 volume to 0dB (0x1E) to hear from the headphones output
    res |= es_write_reg(ES8388_ADDR, ES8388_DACCONTROL26, 0x1E); 
    res |= es_write_reg(ES8388_ADDR, ES8388_DACCONTROL27, 0x1E);

    // other code

    // set 16 Bits length, I2S serial audio data format and I2S channels swap by adjusting the L/R polarity settings in the codec driver
    res |= es_write_reg(ES8388_ADDR, ES8388_ADCCONTROL4, 0x2c);

    // other code

    // set mic left and right channel PGA gain to 0dB (It was set to 0xbb, above +24dB)
    res |= es_write_reg(ES8388_ADDR, ES8388_ADCCONTROL1, 0x00);  

    // other code
}
```

### Menuconfig
Some low-level system features were configured by running `idf.py menuconfig` utility to ensure optimal compatibility and performance on the target hardware.

Due to the fact that `ESP32-A1S Audio Kit v2.2 A436` board wasn't natively listed in the framework's predefined options the Audio HAL configuration was manually set to: "Audio HAL -> Audio board -> **Custom audio board**".

Next, the SPI flash configuration was optimized for speed. The Flash SPI mode was changed to "Serial flasher config -> Flash SPI mode -> **QIO**" that is the fastest solution as it uses 4 pins for address and data and the Flash SPI speed was set to "Serial flasher config -> Flash SPI speed -> **80 MHz**".

To ensure sufficient contiguous memory for the delay buffer and low-latency operation the external PSRAM was configured. The PSRAM access method was configured to "Component config -> ESP PSRAM -> Support for external, SPI-connected RAM -> SPI RAM config -> SPI RAM access method -> **Make RAM allocatable using heap_caps_malloc(..., MALLOC_CAP_SPIRAM)**" enabling the system heap to allocate the delay memory from external RAM. Finally, the RAM clock speed was increased to $80$ MHz to reduce memory access latency and maximize data throughput: "Component config -> ESP PSRAM -> Support for external, SPI-connected RAM -> SPI RAM config -> Set RAM clock speed -> **80MHz clock speed**".
