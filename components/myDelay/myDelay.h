// Copyright 2018 Espressif Systems (Shanghai) PTE LTD
// All rights reserved.

#ifndef _MYDELAY_H_
#define _MYDELAY_H_

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
typedef struct myDelay_cfg {
    int samplerate;  /*!< Audio sample rate (in Hz)*/
    int channel;     /*!< Number of audio channels (Mono=1, Dual=2) */
    int out_rb_size; /*!< Size of output ring buffer */
    int task_stack;  /*!< Task stack size */
    int task_core;   /*!< Task running in core...*/
    int task_prio;   /*!< Task priority*/
} myDelay_cfg_t;

#define MYDELAY_TASK_STACK       (4 * 1024)
#define MYDELAY_TASK_CORE        (0)
#define MYDELAY_TASK_PRIO        (5)
#define MYDELAY_RINGBUFFER_SIZE  (8 * 1024)


#define DEFAULT_MYDELAY_CONFIG() {                \
        .samplerate  = 48000,                      \
        .channel     = 1,                         \
        .out_rb_size = MYDELAY_RINGBUFFER_SIZE,   \
        .task_stack  = MYDELAY_TASK_STACK,        \
        .task_core   = MYDELAY_TASK_CORE,         \
        .task_prio   = MYDELAY_TASK_PRIO,         \
    }

/**
 * @brief      Set the audio sample rate and the number of channels to be processed by the delay.
 *
 * @param      self       Audio element handle
 * @param      rate       Audio sample rate
 * @param      ch         Audio channel
 *
 * @return     
 *             ESP_OK
 *             ESP_FAIL
 */
esp_err_t myDelay_set_info(audio_element_handle_t self, int rate, int ch);

/**
 * @brief      Create an Audio Element handle that delay incoming data.
 *
 * @param      config  The configuration
 *
 * @return     The created audio element handle
 */
audio_element_handle_t myDelay_init(myDelay_cfg_t *config);

#ifdef __cplusplus
}
#endif

#endif