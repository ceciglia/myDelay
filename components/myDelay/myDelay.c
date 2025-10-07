// Copyright 2018 Espressif Systems (Shanghai) PTE LTD
// All rights reserved.

#include <string.h>
#include "esp_log.h"
#include "audio_error.h"
#include "audio_mem.h"
#include "audio_element.h"
#include "myDelay.h"
#include "audio_type_def.h"

static const char *TAG = "MYDELAY";

#define BUF_SIZE (128)

typedef struct myDelay {
    int  samplerate;
    int  channel;
    unsigned char *buf;
    unsigned char *delayMemory; //custom
    int memorySize;     //custom
    int writeIndex;   //custom
    float oldSample; //custom
    float feedback;   //custom
    // LFO_t* modulation; //custom
    int  byte_num;
    int  at_eof;
} myDelay_t;

typedef union {
	short audiosample16;
    int16_t audiosampleint16; //custom
	struct audiosample16_bytes{
		unsigned h	:8;
		unsigned l	:8;
	} audiosample16_bytes;
} audiosample16_t;

#ifdef DEBUG_MYDELAY_ENC_ISSUE
static FILE *infile;
#endif

static esp_err_t is_valid_myDelay_samplerate(int samplerate)
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

static esp_err_t is_valid_myDelay_channel(int channel)
{
    if ((channel != 1)
        && (channel != 2)) {
        ESP_LOGE(TAG, "The number of channels should be only 1 or 2, here is %d. (line %d)", channel, __LINE__);
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

esp_err_t myDelay_set_info(audio_element_handle_t self, int rate, int ch)
{
    myDelay_t *myDelay = (myDelay_t *)audio_element_getdata(self);
    if (myDelay->samplerate == rate && myDelay->channel == ch) {
        return ESP_OK;
    }
    if ((is_valid_myDelay_samplerate(rate) != ESP_OK)
        || (is_valid_myDelay_channel(ch) != ESP_OK)) {
        return ESP_ERR_INVALID_ARG;
    } else {
        ESP_LOGI(TAG, "The reset sample rate and channel of audio stream are %d %d.", rate, ch);
    
        myDelay->samplerate = rate;
        myDelay->channel = ch;

    }
    return ESP_OK;
}

//custom
// esp_err_t myDelay_set_LFO_modulation(audio_element_handle_t self, LFO_cfg_t *modCfg)
// {
//     myDelay_t *myDelay = (myDelay_t *)audio_element_getdata(self);
//     if (modCfg == NULL) {
//         ESP_LOGE(TAG, "modCfg is NULL. (line %d)", __LINE__);
//         return ESP_ERR_INVALID_ARG;
//     }
//     LFO_t *LFO = (LFO_t *)audio_element_getdata(self);
//     if (LFO->samplerate != modCfg->samplerate || LFO->channel != modCfg->channel) {
//         ESP_LOGE(TAG, "LFO sample rate and channel must match modulation config. (line %d)", __LINE__);
//         return ESP_ERR_INVALID_ARG;
//     }
//     myDelay->modulation = LFO;
//     return ESP_OK;
// }
//end custom

static esp_err_t myDelay_destroy(audio_element_handle_t self)
{
    myDelay_t *myDelay = (myDelay_t *)audio_element_getdata(self);
    audio_free(myDelay);
    return ESP_OK;
}

static esp_err_t myDelay_open(audio_element_handle_t self)
{
#ifdef MYDELAY_MEMORY_ANALYSIS
    AUDIO_MEM_SHOW(TAG);
#endif
    ESP_LOGD(TAG, "myDelay_open");
    myDelay_t *myDelay = (myDelay_t *)audio_element_getdata(self);
    audio_element_info_t info = {0};
    audio_element_getinfo(self, &info);
    if (info.sample_rates
        && info.channels) {
        myDelay->samplerate = info.sample_rates;
        myDelay->channel = info.channels;
    }
    myDelay->at_eof = 0;
    if (is_valid_myDelay_samplerate(myDelay->samplerate) != ESP_OK
        || is_valid_myDelay_channel(myDelay->channel) != ESP_OK) {
        return ESP_ERR_INVALID_ARG;
    }
    myDelay->buf = (unsigned char *)calloc(1, BUF_SIZE); //custom
    if (myDelay->buf == NULL) {
        ESP_LOGE(TAG, "calloc buffer failed. (line %d)", __LINE__);
        return ESP_ERR_NO_MEM;
    }
    memset(myDelay->buf, 0, BUF_SIZE); 

    // // work in bytes
    myDelay->memorySize = (int)(MYDELAY_MAX_DELAY_TIME * myDelay->samplerate)  + BUF_SIZE / sizeof(int16_t) ; //custom: è in campioni
    
    // size_t delayBufferSize = myDelay->memorySize * sizeof(int16_t); //custom
    // ESP_LOGI(TAG, "myDelay memory size in samples: %d", delayBufferSize);
    // //custom
    // myDelay->delayMemory = (unsigned char *)calloc(1, BUF_SIZE); //custom
    // if (myDelay->delayMemory == NULL) {
    //     ESP_LOGE(TAG, "calloc buffer failed. (line %d)", __LINE__);
    //     return ESP_ERR_NO_MEM;
    // }
    // memset(myDelay->delayMemory, 0, BUF_SIZE);
    
    //versione con PSRAM
    size_t delay_bytes = (size_t)myDelay->memorySize * sizeof(int16_t); //custom: è in byte
    ESP_LOGI(TAG, "myDelay memory size in bytes: %u", (unsigned)delay_bytes);

    // Sostituisci calloc() con heap_caps_calloc() e il flag MALLOC_CAP_SPIRAM
    myDelay->delayMemory = (unsigned char *)heap_caps_calloc(1, delay_bytes, MALLOC_CAP_SPIRAM); 
    if (myDelay->delayMemory == NULL) {
        ESP_LOGE(TAG, "calloc FAILED! Could not allocate %u bytes in PSRAM.", delay_bytes);
        // Pulizia
        audio_free(myDelay->buf); 
        myDelay->buf = NULL;
        return ESP_ERR_NO_MEM;
    }
    // Azzeramento usando il numero di byte (funziona anche su PSRAM)
    memset(myDelay->delayMemory, 0, delay_bytes);
    // end versione con PSRAM



    myDelay->feedback = 0.5f; //custom
    myDelay->oldSample = 0.0f; //custom

    //end custom

#ifdef DEBUG_MYDELAY_ENC_ISSUE
    char fileName[100] = {'//', 's', 'd', 'c', 'a', 'r', 'd', '//', 't', 'e', 's', 't', '.', 'p', 'c', 'm', '\0'};
    infile = fopen(fileName, "rb");
    if (!infile) {
        perror(fileName);
        return ESP_FAIL;
    }
#endif

    return ESP_OK;
}

static esp_err_t myDelay_close(audio_element_handle_t self)
{
    ESP_LOGD(TAG, "myDelay_close");
    myDelay_t *myDelay = (myDelay_t *)audio_element_getdata(self);
    if(myDelay->buf != NULL){ //custom
        audio_free(myDelay->buf);
        myDelay->buf = NULL;
    }  
    
    //custom
    if(myDelay->delayMemory != NULL){
        audio_free(myDelay->delayMemory);
        myDelay->delayMemory = NULL;
    }  
    //end custom

#ifdef MYDELAY_MEMORY_ANALYSIS
    AUDIO_MEM_SHOW(TAG);
#endif
#ifdef DEBUG_MYDELAY_ENC_ISSUE
    fclose(infile);
#endif

    return ESP_OK;
}

static int myDelay_process(audio_element_handle_t self, char *in_buffer, int in_len)
{
    myDelay_t *myDelay = (myDelay_t *)audio_element_getdata(self);
    int ret = 0;

    int r_size = 0;
    if (myDelay->at_eof == 0) {
#ifdef DEBUG_MYDELAY_ENC_ISSUE
        r_size = fread((char *)myDelay->buf, 1, BUF_SIZE, infile);
#else
        r_size = audio_element_input(self, (char *)myDelay->buf, BUF_SIZE); //custom
#endif
    }
    if (r_size > 0) {    
        if (r_size != BUF_SIZE) { 
            myDelay->at_eof = 1;
        }
        myDelay->byte_num += r_size;

        // unsigned char *pbuf = myDelay->buf;
        // unsigned char *pDelayMem = myDelay->delayMemory; //custom
        // unsigned char *pMod = myDelay->modulation.buf; //custom ERROREEEEEEEEEEEEEEEEEEEEE

        int16_t *pbuf16 = (int16_t *)myDelay->buf; //custom
        int16_t *pDelayMem16 = (int16_t *)myDelay->delayMemory; //custom

        audiosample16_t audiosample;
        float dt = 0.5f; //custom da cancellare

        for(int i=0; i<r_size / 2; i++){ //custom 
            // vers 1
            float inputSample = (float)pbuf16[i] / 32767.0f; //custom
            float readIndex = (float)myDelay->writeIndex - (dt * (float)myDelay->samplerate) ; //custom
            int integerPart = (int) readIndex; //custom
            float fractionalPart = readIndex - integerPart; //custom
            float alpha = fractionalPart / (2.0f - fractionalPart); //custom
            
            int A = (integerPart + myDelay->memorySize) % myDelay->memorySize; //custom
            int B = (A + 1) % myDelay->memorySize; //custom

            float sample_A_float = (float)pDelayMem16[A] / 32767.0f;
            float sample_B_float = (float)pDelayMem16[B] / 32767.0f;
            float sampleValue = alpha * (sample_B_float - myDelay->oldSample) + sample_A_float; 
            
            myDelay->oldSample = sampleValue; //custom

            float dw = 0.5f; // dry wet mix
            float outputSample = sampleValue * dw + inputSample * (1.0f - dw); //custom

            // controllo
            if (outputSample > 1.0f) outputSample = 1.0f;
            else if (outputSample < -1.0f) outputSample = -1.0f;

            audiosample.audiosampleint16 = (int16_t)(outputSample * 32767.0f);
            pbuf16[i] = audiosample.audiosampleint16;


            // pDelayMem16[myDelay->writeIndex] = pDelayMem16[myDelay->writeIndex] + pbuf16[i] * (int16_t)(myDelay->feedback * 32767.0f);
            pDelayMem16[myDelay->writeIndex] = pbuf16[i];

            myDelay->writeIndex = (myDelay->writeIndex + 1) % myDelay->memorySize; //custom
            // end vers 1
        }
        ret = audio_element_output(self, (char *)myDelay->buf, BUF_SIZE);
    } else {
        ret = r_size;
    }
    return ret;
}

audio_element_handle_t myDelay_init(myDelay_cfg_t *config)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "config is NULL. (line %d)", __LINE__);
        return NULL;
    }
    myDelay_t *myDelay = audio_calloc(1, sizeof(myDelay_t));
    AUDIO_MEM_CHECK(TAG, myDelay, return NULL);     
    if (myDelay == NULL) {
        ESP_LOGE(TAG, "audio_calloc failed for myDelay. (line %d)", __LINE__);
        return NULL;
    }
    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    cfg.destroy = myDelay_destroy;
    cfg.process = myDelay_process;
    cfg.open = myDelay_open;
    cfg.close = myDelay_close;
    cfg.buffer_len = 0;
    cfg.tag = "myDelay";
    cfg.task_stack = config->task_stack;
    cfg.task_prio = config->task_prio;
    cfg.task_core = config->task_core;
    cfg.out_rb_size = config->out_rb_size;
    audio_element_handle_t el = audio_element_init(&cfg);
    AUDIO_MEM_CHECK(TAG, el, {audio_free(myDelay); return NULL;});
    myDelay->samplerate = config->samplerate;
    myDelay->channel = config->channel;
    audio_element_setdata(el, myDelay);
    audio_element_info_t info = {0};
    audio_element_setinfo(el, &info);
    ESP_LOGD(TAG, "myDelay_init");
    return el;
}
