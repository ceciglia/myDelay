// Copyright 2018 Espressif Systems (Shanghai) PTE LTD
// All rights reserved.

#include <string.h>
#include "esp_log.h"
#include "audio_error.h"
#include "audio_mem.h"
#include "audio_element.h"
#include "LFO.h"
#include "audio_type_def.h"

static const char *TAG = "LFO";

// #define BUF_SIZE (128)
#define BUF_SIZE (1024) // custom value

// typedef struct LFO {
//     int  samplerate;
//     int  channel;
//     float frequency; //custom: posso usare i float?
//     int waveform;  //custom
//     float currentPhase; //custom
//     float samplingPeriod; //custom
//     // unsigned char *buf;
//     // int  byte_num;
//     // int  at_eof;
// } LFO_t;

// typedef union {
// 	short audiosample16;
//     int16_t audiosampleint16; //custom
// 	struct audiosample16_bytes{
// 		unsigned h	:8;
// 		unsigned l	:8;
// 	} audiosample16_bytes;
// } audiosample16_t;

#ifdef DEBUG_LFO_ENC_ISSUE
static FILE *infile;
#endif

static esp_err_t is_valid_LFO_samplerate(int samplerate)
{
    if ((samplerate != 11025)
        && (samplerate != 22050)
        && (samplerate != 44100)
        && (samplerate != 48000)) {
        ESP_LOGE(TAG, "The sample rate should be only 11025Hz, 22050Hz, 44100Hz, 48000Hz, here is %dHz. (line %d)", samplerate, __LINE__);
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

static esp_err_t is_valid_LFO_channel(int channel) // CHECK: LFO probabilmente sarà mono
{
    if ((channel != 1)
        && (channel != 2)) {
        ESP_LOGE(TAG, "The number of channels should be only 1 or 2, here is %d. (line %d)", channel, __LINE__);
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

esp_err_t LFO_set_info(audio_element_handle_t self, int rate, int ch)
{
    LFO_t *LFO = (LFO_t *)audio_element_getdata(self);
    if (LFO->samplerate == rate && LFO->channel == ch) {
        return ESP_OK;
    }
    if ((is_valid_LFO_samplerate(rate) != ESP_OK)
        || (is_valid_LFO_channel(ch) != ESP_OK)) {
        return ESP_ERR_INVALID_ARG;
    } else {
        ESP_LOGI(TAG, "The reset sample rate and channel of audio stream are %d %d.", rate, ch);
    
        LFO->samplerate = rate;
        LFO->channel = ch;
        
    }
    return ESP_OK;
}

static esp_err_t LFO_destroy(audio_element_handle_t self)
{
    LFO_t *LFO = (LFO_t *)audio_element_getdata(self);
    audio_free(LFO);
    return ESP_OK;
}

static esp_err_t LFO_open(audio_element_handle_t self)
{
#ifdef LFO_MEMORY_ANALYSIS
    AUDIO_MEM_SHOW(TAG);
#endif
    ESP_LOGD(TAG, "LFO_open");
    LFO_t *LFO = (LFO_t *)audio_element_getdata(self);
    audio_element_info_t info = {0};
    audio_element_getinfo(self, &info);
    if (info.sample_rates
        && info.channels) {
        LFO->samplerate = info.sample_rates;
        LFO->channel = info.channels;
    }

    // LFO->at_eof = 0;

    if (is_valid_LFO_samplerate(LFO->samplerate) != ESP_OK
        || is_valid_LFO_channel(LFO->channel) != ESP_OK) {
        return ESP_ERR_INVALID_ARG;
    }

    // LFO->buf = (unsigned char *)calloc(1, BUF_SIZE);
    // if (LFO->buf == NULL) {
    //     ESP_LOGE(TAG, "calloc buffer failed. (line %d)", __LINE__);
    //     return ESP_ERR_NO_MEM;
    // }
    // memset(LFO->buf, 0, BUF_SIZE);

    LFO->currentPhase = 0.0f; 
    LFO->samplingPeriod = 1.0f / (float)LFO->samplerate;
    LFO->frequency = 5.0f; //custom
    LFO->waveform = 2;  //custom: triangle wave
    LFO->modAmount = 0.02f; //custom: modulation amount

    LFO->debugCount = 0; //DEBUG DA CANCELLARE
    ESP_LOGI(TAG, "ci siamo arrivatiiiiiiiiiiiiiii");
    
#ifdef DEBUG_LFO_ENC_ISSUE
    char fileName[100] = {'//', 's', 'd', 'c', 'a', 'r', 'd', '//', 't', 'e', 's', 't', '.', 'p', 'c', 'm', '\0'};
    infile = fopen(fileName, "rb");
    if (!infile) {
        perror(fileName);
        return ESP_FAIL;
    }
#endif

    return ESP_OK;
}

static esp_err_t LFO_close(audio_element_handle_t self)
{
    ESP_LOGD(TAG, "LFO_close");
    // LFO_t *LFO = (LFO_t *)audio_element_getdata(self);
    // if(LFO->buf == NULL){
    //     audio_free(LFO->buf);
    //     LFO->buf = NULL;
    // }   

#ifdef LFO_MEMORY_ANALYSIS
    AUDIO_MEM_SHOW(TAG);
#endif
#ifdef DEBUG_LFO_ENC_ISSUE
    fclose(infile);
#endif

    return ESP_OK;
}

//custom
esp_err_t LFO_set_waveform(audio_element_handle_t self, int type)
{
    LFO_t *LFO = (LFO_t *)audio_element_getdata(self);
    if (type < 0 || type > 3) {
        ESP_LOGE(TAG, "Invalid waveform type. (line %d)", __LINE__);
        return ESP_ERR_INVALID_ARG;
    }
    LFO->waveform = type;
    ESP_LOGI(TAG, "LFO waveform set to type %d", type);
    return ESP_OK;
}

esp_err_t LFO_set_frequency(audio_element_handle_t self, float newFreq)
{
    LFO_t *LFO = (LFO_t *)audio_element_getdata(self);
    if (newFreq <= 0.0f || newFreq > (LFO->samplerate / 2.0f)) {
        ESP_LOGE(TAG, "Invalid frequency value. (line %d)", __LINE__);
        return ESP_ERR_INVALID_ARG;
    }
    LFO->frequency = newFreq;
    ESP_LOGI(TAG, "LFO frequency set to %.2f Hz", newFreq);
    return ESP_OK;
}

// esp_err_t LFO_get_next_sample(LFO_t *LFO, float *outSample) 
// {
//     // LFO_t *LFO = (LFO_t *)audio_element_getdata(self);
//     float sample = 0.0f;
//     // Calculate the next sample based on the waveform type
//     switch (LFO->waveform) {
//         case 0: // Sine wave
//             sample = sinf(LFO->currentPhase * 2.0f * M_PI);
//             break;
//         case 1: // Square wave
//             sample = (LFO->currentPhase >= 0.5f) ? 1.0f : -1.0f;
//             break;
//         case 2: // Triangle wave
//             sample = 4.0f * fabsf(LFO->currentPhase - 0.5f) - 1.0f; //check
//             break;
//         case 3: // Sawtooth wave (rising)
//             sample = 2.0f * LFO->currentPhase - 1.0f; //check
//             break;
//         default:
//             // sample = 0.0f; // Default to silence for unknown waveform types
//             break;
//     }
    
//     float phaseIncrement = LFO->frequency * LFO->samplingPeriod;
//     LFO->currentPhase += phaseIncrement;
//     LFO->currentPhase -= (int)LFO->currentPhase;
//     get_unipolar_LFO(&sample); // Scale to [0, 1] range
//     *outSample = sample * LFO->modAmount; // Scale by modulation amount
//     if (LFO->debugCount%100000 == 0) {
//         ESP_LOGI(TAG, "LFO frequency: %.2f Hz", LFO->frequency);
//         ESP_LOGI(TAG, "LFO waveform type: %d", LFO->waveform);  
//         ESP_LOGI(TAG, "LFO phaseincrement: %.6f", phaseIncrement);
//         ESP_LOGI(TAG, "LFO currentPhase: %.6f", LFO->currentPhase);
//         ESP_LOGI(TAG, "LFO sample: %.3f", sample);
//         ESP_LOGI(TAG, "LFO *outSample: %.3f", *outSample);
//     }
//     LFO->debugCount++;
//     return ESP_OK;
// }

esp_err_t LFO_prepare_to_play(audio_element_handle_t self) 
{
    LFO_t *LFO = (LFO_t *)audio_element_getdata(self);
    if (LFO->ciao != 1) {
        LFO->currentPhase = 0.0f; 
        LFO->samplingPeriod = 1.0f / (float)LFO->samplerate;
        LFO->frequency = 5.0f; //custom
        LFO->waveform = 2;  //custom: triangle wave
        LFO->modAmount = 0.02f; //custom: modulation amount 
        LFO->ciao = 1;
    }
    return ESP_OK;
}

float LFO_get_next_sample(audio_element_handle_t self) 
{
    LFO_t *LFO = (LFO_t *)audio_element_getdata(self);
    LFO_prepare_to_play(self);
    float sample = 0.0f;
    // Calculate the next sample based on the waveform type
    switch (LFO->waveform) {
        case 0: // Sine wave
            sample = sinf(LFO->currentPhase * 2.0f * M_PI);
            break;
        case 1: // Square wave
            sample = (LFO->currentPhase >= 0.5f) ? 1.0f : -1.0f;
            break;
        case 2: // Triangle wave
            sample = 4.0f * fabsf(LFO->currentPhase - 0.5f) - 1.0f; //check
            break;
        case 3: // Sawtooth wave (rising)
            sample = 2.0f * LFO->currentPhase - 1.0f; //check
            break;
        default:
            // sample = 0.0f; // Default to silence for unknown waveform types
            break;
    }
    
    float phaseIncrement = LFO->frequency * LFO->samplingPeriod;
    LFO->currentPhase += phaseIncrement;
    LFO->currentPhase -= (int)LFO->currentPhase;
    get_unipolar_LFO(&sample); // Scale to [0, 1] range
    sample = sample * LFO->modAmount; // Scale by modulation amount
    // if (LFO->debugCount%10000000 == 0) {
    //     ESP_LOGI(TAG, "LFO frequency: %.2f Hz", LFO->frequency);
    //     ESP_LOGI(TAG, "LFO waveform type: %d", LFO->waveform);  
    //     ESP_LOGI(TAG, "LFO phaseincrement: %.6f", phaseIncrement);
    //     ESP_LOGI(TAG, "LFO currentPhase: %.6f", LFO->currentPhase);
    //     ESP_LOGI(TAG, "LFO sample: %.3f", sample);
    // }
    // LFO->debugCount++;
    return sample;
}

esp_err_t get_unipolar_LFO(float *sample) 
{
    // normalize to [0,1]
    *sample += 1.0f; 
    *sample *= 0.5f;
    return ESP_OK;
}

//end custom

static int LFO_process(audio_element_handle_t self, char *in_buffer, int in_len)
{
    // int r_size = audio_element_input(self, in_buffer, in_len);
    // if (r_size > 0) {
    //     // LFO_t *LFO = (LFO_t *)audio_element_getdata(self);
    //     // float nextSample = 0.0f;
    //     // nextSample = LFO_get_next_sample(self);
    //     // LFO->currentValue = nextSample;
    
    //     // vTaskDelay(pdMS_TO_TICKS(10));
    
    //     // return 0;
    //     audio_element_output(self, in_buffer, r_size);
    // }
    // return r_size;
    
    //custom
//     LFO_t *LFO = (LFO_t *)audio_element_getdata(self);
//     int ret = 0;

//     int r_size = 0;
//     if (LFO->at_eof == 0) {
// #ifdef DEBUG_LFO_ENC_ISSUE
//         r_size = fread((char *)LFO->buf, 1, BUF_SIZE, infile);
// #else
// r_size = audio_element_input(self, (char *)LFO->buf, BUF_SIZE); //custom
// #endif
// }
// if (r_size > 0) {    
//     if (r_size != BUF_SIZE) { 
//         LFO->at_eof = 1;
//     }
//     LFO->byte_num += r_size;
    
//     int16_t *LFObuf16 = (int16_t *)LFO->buf; //custom
    
//     for(int i=0; i<r_size / 2; i+=2) { //custom 
//         for (int j=0; j<LFO->channel; j++) { //custom
//             LFObuf16[i+j] = get
//         }
//     }
//         ret = audio_element_output(self, (char *)myDelay->buf, BUF_SIZE);
//     } else {
//         ret = r_size;
//     }
//     return ret;
    return 0;
    // end custom
}

audio_element_handle_t LFO_init(LFO_cfg_t *config)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "config is NULL. (line %d)", __LINE__);
        return NULL;
    }
    LFO_t *LFO = audio_calloc(1, sizeof(LFO_t));
    AUDIO_MEM_CHECK(TAG, LFO, return NULL);     
    if (LFO == NULL) {
        ESP_LOGE(TAG, "audio_calloc failed for LFO. (line %d)", __LINE__);
        return NULL;
    }
    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    cfg.destroy = LFO_destroy;
    cfg.process = LFO_process;
    cfg.open = LFO_open;
    cfg.close = LFO_close;
    cfg.buffer_len = 0;
    cfg.tag = "LFO";
    cfg.task_stack = config->task_stack;
    cfg.task_prio = config->task_prio;
    cfg.task_core = config->task_core;
    cfg.out_rb_size = config->out_rb_size;
    audio_element_handle_t el = audio_element_init(&cfg);
    AUDIO_MEM_CHECK(TAG, el, {audio_free(LFO); return NULL;});
    LFO->samplerate = config->samplerate;
    LFO->channel = config->channel;
    audio_element_setdata(el, LFO);
    audio_element_info_t info = {0};
    audio_element_setinfo(el, &info);
    ESP_LOGD(TAG, "LFO_init");
    return el;
}


//custom

// esp_err_t LFO_create(LFO_t *LFO, int samplerate, int channel) 
// {
//     if (LFO == NULL) {
//         ESP_LOGE(TAG, "audio_calloc failed for LFO. (line %d)", __LINE__);
//         return ESP_FAIL;
//     }
//     LFO->samplerate = samplerate;
//     LFO->channel = channel;
//     LFO->currentPhase = 0.0f; 
//     LFO->samplingPeriod = 1.0f / (float)LFO->samplerate;
//     LFO->frequency = 5.0f; //custom
//     LFO->waveform = 2;  //custom: triangle wave
//     LFO->modAmount = 0.02f; //custom: modulation amount

//     LFO->debugCount = 0; //DEBUG DA CANCELLARE
//     ESP_LOGI(TAG, "LFO setup completed.");
//     return ESP_OK;
// }

//end custom