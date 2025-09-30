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
    unsigned char *delayMemory
    int  byte_num;
    int  at_eof;
} myDelay_t;

typedef union {
	short audiosample16;
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
    myDelay->buf = (unsigned char *)calloc(1, BUF_SIZE);
    if (myDelay->buf == NULL) {
        ESP_LOGE(TAG, "calloc buffer failed. (line %d)", __LINE__);
        return ESP_ERR_NO_MEM;
    }
    memset(myDelay->buf, 0, BUF_SIZE);

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
    if(myDelay->buf == NULL){
        audio_free(myDelay->buf);
        myDelay->buf = NULL;
    }   

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
    int dm_size = 0;    //custom
    if (myDelay->at_eof == 0) {
#ifdef DEBUG_MYDELAY_ENC_ISSUE
        r_size = fread((char *)myDelay->buf, 1, BUF_SIZE, infile);
        dm_size = fread((char *)myDelay->delayMemory, 1, BUF_SIZE, infile); //custom
#else
        r_size = audio_element_input(self, (char *)myDelay->buf, BUF_SIZE);
#endif
    }
    if (r_size > 0 ||) {
        if (r_size != BUF_SIZE) {
            myDelay->at_eof = 1;
        }
        myDelay->byte_num += r_size;

        unsigned char *pbuf = myDelay->buf;
        unsigned char *pDelayMem = myDelay->delayMemory;

        for(int i=0;i<r_size;i+=2){
            audiosample16_t audiosample;
            audiosample.audiosample16_bytes.h = *pbuf;
            audiosample.audiosample16_bytes.l = *(pbuf+1);

            // process audio samples here e.g. scale by 1/16 (attenuation):
            // audiosample.audiosample16 = audiosample.audiosample16>>4;

            *pbuf = audiosample.audiosample16_bytes.h;
            *(pbuf+1) = audiosample.audiosample16_bytes.l;
            pbuf+=2;
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
