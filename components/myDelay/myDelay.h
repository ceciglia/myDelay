// Copyright 2018 Espressif Systems (Shanghai) PTE LTD
// All rights reserved.

#ifndef _MYDELAY_H_
#define _MYDELAY_H_

#include "esp_err.h"
#include <math.h>
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

// #define MYDELAY_TASK_STACK       (4 * 1024)
#define MYDELAY_TASK_STACK       (6 * 1024) //custom
#define MYDELAY_TASK_CORE        (0)
// #define MYDELAY_TASK_PRIO        (5)
#define MYDELAY_TASK_PRIO        (20) //custom
#define MYDELAY_RINGBUFFER_SIZE  (8 * 1024)

#define MYDELAY_MAX_DELAY_TIME       (5.0f) //custom

#define DEFAULT_MYDELAY_CONFIG() {                \
        .samplerate  = 48000,                      \
        .channel     = 2,                         \
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
 * @brief      Set the feedback amount for the delay effect.
 *
 * @param      self         Audio element handle
 * @param      new_feedback  The new feedback value (0.0 to 0.95)
 *
 * @return     
 *             ESP_OK
 *             ESP_FAIL
 */
esp_err_t myDelay_set_feedback(audio_element_handle_t self, float new_feedback); //custom

/**
 * @brief      Set the base delay time for the delay effect.
 *
 * @param      self         Audio element handle
 * @param      new_base_dt    The new base delay time in seconds
 *
 * @return     
 *             ESP_OK
 *             ESP_FAIL
 */
esp_err_t myDelay_set_base_dt_target(audio_element_handle_t self, float new_base_dt); //custom

/**
 * @brief      Set the dry/wet ratio for the delay effect.
 *
 * @param      self         Audio element handle
 * @param      new_dw_ratio  The new dry/wet ratio
 *
 * @return     
 *             ESP_OK
 *             ESP_FAIL
 */
esp_err_t myDelay_set_dw_ratio(audio_element_handle_t self, float new_dw_ratio); //custom

/**
 * @brief      Get the base delay time target for the delay effect.
 *
 * @param      self         Audio element handle
 *
 * @return     The base delay time target in seconds
 */
float myDelay_get_base_dt_target(audio_element_handle_t self); //custom

/**
 * @brief      Get the feedback amount for the delay effect.
 *
 * @param      self         Audio element handle
 *
 * @return     The feedback amount
 */
float myDelay_get_feedback(audio_element_handle_t self); //custom

/**
 * @brief      Get the dry/wet ratio for the delay effect.
 *
 * @param      self         Audio element handle
 *
 * @return     The dry/wet ratio
 */
float myDelay_get_dw_ratio(audio_element_handle_t self); //custom

/**
 * @brief      Set the LFO frequency for the delay effect.
 *
 * @param      self         Audio element handle
 * @param      new_frequency  The new LFO frequency in Hz
 *
 * @return     
 *             ESP_OK
 *             ESP_FAIL
 */
esp_err_t myDelay_set_LFO_frequency(audio_element_handle_t self, float new_frequency); //custom

/**
 * @brief      Set the LFO waveform type for the delay effect.
 *
 * @param      self         Audio element handle
 * @param      new_waveform  The new LFO waveform type (0: Sine, 1: Square, 2: Triangle, 3: Sawtooth)
 *
 * @return     
 *             ESP_OK
 *             ESP_FAIL
 */
esp_err_t myDelay_set_LFO_waveform(audio_element_handle_t self, int new_waveform); //custom

/**
 * @brief      Set the LFO modulation amount for the delay effect.
 *
 * @param      self         Audio element handle
 * @param      new_mod_amount  The new LFO modulation amount
 *
 * @return     
 *             ESP_OK
 *             ESP_FAIL
 */
esp_err_t myDelay_set_LFO_mod_amount(audio_element_handle_t self, float new_mod_amount); //custom

/**
 * @brief      Get the LFO frequency for the delay effect.
 *
 * @param      self         Audio element handle
 *
 * @return     The LFO frequency in Hz
 */
float myDelay_get_LFO_frequency(audio_element_handle_t self); //custom

/**
 * @brief      Get the LFO waveform type for the delay effect.
 *
 * @param      self         Audio element handle
 *
 * @return     The LFO waveform type (0: Sine, 1: Square, 2: Triangle, 3: Sawtooth)
 */
int myDelay_get_LFO_waveform(audio_element_handle_t self); //custom

/**
 * @brief      Get the LFO modulation amount for the delay effect.
 *
 * @param      self         Audio element handle
 *
 * @return     The LFO modulation amount
 */
float myDelay_get_LFO_mod_amount(audio_element_handle_t self); //custom

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