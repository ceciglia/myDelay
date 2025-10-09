// Copyright 2018 Espressif Systems (Shanghai) PTE LTD
// All rights reserved.

#ifndef _MYDELAY_H_
#define _MYDELAY_H_

#include "esp_err.h"
#include "audio_element.h"
#include "audio_common.h"
#include "LFO.h" //custom


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

#define MYDELAY_MAX_DELAY_TIME       (1.5f) //custom


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

// /**
//  * @brief      Set the LFO modulation parameters.
//  *
//  * @param      self       Audio element handle
//  * @param      modCfg     The LFO modulation configuration
//  *
//  * @return     
//  *             ESP_OK
//  *             ESP_FAIL
//  */
// esp_err_t myDelay_set_LFO_modulation(audio_element_handle_t self, LFO_cfg_t *modCfg); //custom

/**
 * @brief      Set the feedback amount for the delay effect.
 *
 * @param      self         Audio element handle
 * @param      newFeedback  The new feedback value (0.0 to 0.95)
 *
 * @return     
 *             ESP_OK
 *             ESP_FAIL
 */
esp_err_t myDelay_set_feedback(audio_element_handle_t self, float newFeedback); //custom

/**
 * @brief 	   Assign the LFO handle to the Delay element for modulation.
 *
 * @param 	   self 	    Audio element handle (myDelay)
 * @param 	   lfo_handle   Audio element handle (LFO)
 *
 * @return 	   ESP_OK or ESP_FAIL
 */
esp_err_t myDelay_set_LFO_handle(audio_element_handle_t self, audio_element_handle_t lfo_handle);

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