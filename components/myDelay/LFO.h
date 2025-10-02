// Copyright 2018 Espressif Systems (Shanghai) PTE LTD
// All rights reserved.

#ifndef _LFO_H_
#define _LFO_H_

#include <math.h>
#include "esp_err.h"
#include "audio_element.h"
#include "audio_common.h"


#ifdef __cplusplus
extern "C"
{
#endif


/**
 * @brief      Delay Configuration
 */
typedef struct LFO_cfg {
    int samplerate;  /*!< Audio sample rate (in Hz)*/
    int channel;     /*!< Number of audio channels (Mono=1, Dual=2) */
    int out_rb_size; /*!< Size of output ring buffer */
    int task_stack;  /*!< Task stack size */
    int task_core;   /*!< Task running in core...*/
    int task_prio;   /*!< Task priority*/
} LFO_cfg_t;

#define LFO_TASK_STACK       (4 * 1024)
#define LFO_TASK_CORE        (0)
#define LFO_TASK_PRIO        (5)
#define LFO_RINGBUFFER_SIZE  (8 * 1024)


#define DEFAULT_LFO_CONFIG() {                \
        .samplerate  = 48000,                 \
        .channel     = 1,                     \
        .out_rb_size = LFO_RINGBUFFER_SIZE,   \
        .task_stack  = LFO_TASK_STACK,        \
        .task_core   = LFO_TASK_CORE,         \
        .task_prio   = LFO_TASK_PRIO,         \
    }

/**
 * @brief      Set the audio sample rate and the number of channels to be processed by the LFO.
 *
 * @param      self       Audio element handle
 * @param      rate       Audio sample rate
 * @param      ch         Audio channel
 *
 * @return     
 *             ESP_OK
 *             ESP_FAIL
 */
esp_err_t LFO_set_info(audio_element_handle_t self, int rate, int ch);

/**
 * @brief      Set the audio waveform.
 *
 * @param      self       Audio element handle
 * @param      type       New waveform type
 *
 * @return     
 *             ESP_OK
 *             ESP_FAIL
 */
esp_err_t LFO_set_waveform(audio_element_handle_t self, int type);

/**
 * @brief      Set the audio frequency.
 *
 * @param      self       Audio element handle
 * @param      freq       New frequency
 *
 * @return     
 *             ESP_OK
 *             ESP_FAIL
 */
esp_err_t LFO_set_frequency(audio_element_handle_t self, float freq);

/**
 * @brief      Create an Audio Element handle that LFO incoming data.
 *
 * @param      config  The configuration
 *
 * @return     The created audio element handle
 */
audio_element_handle_t LFO_init(LFO_cfg_t *config);

#ifdef __cplusplus
}
#endif

#endif