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

// #define BUF_SIZE (128)
#define BUF_SIZE (512) // custom value

typedef struct LFO {
    int  samplerate;
    int  channel;
    float frequency; //custom: posso usare i float?
    int waveform;  //custom
    float currentPhase; //custom
    float samplingPeriod; //custom
    float modAmount; //custom
    int debugCount; //DEBUG DA CANCELLARE
    // unsigned char *buf;
    // float currentValue; //custom
    // int ciao;
    // int  byte_num;
    // int  at_eof;
} LFO_t;

typedef struct myDelay {
    int  samplerate;
    int  channel;
    unsigned char *buf;
    unsigned char *delayMemory; //custom
    int memorySize;     //custom
    int writeIndex;   //custom
    float oldSample[2]; //custom
    float feedback;   //custom
    LFO_t *LFO_handle; //custom
    int debug; //custom da cancellare
    float max; //custom da cancellare
    float min; //custom da cancellare
    int  byte_num;
    int  at_eof;
} myDelay_t;

// typedef union {
// 	short audiosample16;
//     int16_t audiosampleint16; //custom
// 	struct audiosample16_bytes{
// 		unsigned h	:8;
// 		unsigned l	:8;
// 	} audiosample16_bytes;
// } audiosample16_t;

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

// esp_err_t myDelay_set_LFO_handle(audio_element_handle_t self, audio_element_handle_t lfo_handle) {
//     myDelay_t *myDelay = (myDelay_t *)audio_element_getdata(self);
//     if (lfo_handle == NULL) {
//         ESP_LOGE(TAG, "lfo_handle is NULL. (line %d)", __LINE__);
//         return ESP_ERR_INVALID_ARG;
//     }
    
//     LFO_t *LFO = (LFO_t *)audio_element_getdata(lfo_handle);
//     if (LFO == NULL) {
//         ESP_LOGE(TAG, "The provided handle is not a valid LFO element. (line %d)", __LINE__);
//         return ESP_ERR_INVALID_ARG;
//     }
    
//     myDelay->LFO_handle = lfo_handle;
//     ESP_LOGI(TAG, "LFO handle assigned to myDelay.");
    
//     if (LFO->samplerate != myDelay->samplerate || LFO->channel != myDelay->channel) {
//         LFO->samplerate = myDelay->samplerate; 
//         LFO->channel = myDelay->channel;
//         ESP_LOGI(TAG, "LFO sample rate and channel adjusted to match myDelay config.");
//     }

//     return ESP_OK;
// }

esp_err_t myDelay_set_feedback(audio_element_handle_t self, float newFeedback) {
    myDelay_t *myDelay = (myDelay_t *)audio_element_getdata(self);
    if (newFeedback < 0.0f || newFeedback > 0.95f) {
        ESP_LOGE(TAG, "Feedback must be between 0.0 and 0.95. (line %d)", __LINE__);
        return ESP_ERR_INVALID_ARG;
    }
    myDelay->feedback = newFeedback;
    ESP_LOGI(TAG, "Feedback set to %.2f", newFeedback);
    return ESP_OK;
}

// LFO

esp_err_t LFO_prepare_to_play(LFO_t *LFO, audio_element_handle_t myD) 
{
    // LFO_t *LFO = (LFO_t *)audio_element_getdata(self);
    myDelay_t *myDelay = (myDelay_t *)audio_element_getdata(myD);
    LFO->samplerate = myDelay->samplerate;
    LFO->channel = 1; // LFO is always mono
    LFO->currentPhase = 0.0f; 
    LFO->samplingPeriod = 1.0f / (float)LFO->samplerate;
    LFO->frequency = 0.1f; //custom
    LFO->waveform = 2;  //custom: triangle wave
    LFO->modAmount = 0.001f; //custom: modulation amount
    LFO->debugCount = 0; //DEBUG DA CANCELLARE
    ESP_LOGI(TAG, "LFO setup completed.");
    ESP_LOGI(TAG, "LFO sampling period: %.6f", LFO->samplingPeriod);
    return ESP_OK;
}

esp_err_t get_unipolar_LFO(float *sample) 
{
    // normalize to [0,1]
    *sample += 1.0f; 
    *sample *= 0.5f;
    return ESP_OK;
}

float LFO_get_next_sample(LFO_t *LFO) 
{
    // LFO_t *LFO = (LFO_t *)audio_element_getdata(self);
    // LFO_prepare_to_play(self);
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
    // if (LFO->debugCount%100000 == 0) {
        // ESP_LOGI(TAG, "LFO frequency: %.2f Hz", LFO->frequency);
        // ESP_LOGI(TAG, "LFO waveform type: %d", LFO->waveform);  
        // ESP_LOGI(TAG, "LFO phaseincrement: %.6f", phaseIncrement);
        // ESP_LOGI(TAG, "LFO currentPhase: %.6f", LFO->currentPhase);
        // ESP_LOGI(TAG, "LFO sample: %.3f", sample);
    // }
    // LFO->debugCount++;
    return sample;
}

// end LFO


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

    myDelay->memorySize = (int)(MYDELAY_MAX_DELAY_TIME * myDelay->samplerate)  + BUF_SIZE / sizeof(int16_t) ; //custom: è in campioni
    myDelay->memorySize = myDelay->memorySize * myDelay->channel; //custom!!! la raddoppio se sono in stereo perché ho i canali interleaved
    
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

    // Sostituito calloc() con heap_caps_calloc() e il flag MALLOC_CAP_SPIRAM
    myDelay->delayMemory = (unsigned char *)heap_caps_calloc(1, delay_bytes, MALLOC_CAP_SPIRAM); 
    if (myDelay->delayMemory == NULL) {
        ESP_LOGE(TAG, "calloc FAILED! Could not allocate %u bytes in PSRAM.", delay_bytes);
        audio_free(myDelay->buf); 
        myDelay->buf = NULL;
        return ESP_ERR_NO_MEM;
    }
    memset(myDelay->delayMemory, 0, delay_bytes);
    // end versione con PSRAM

    LFO_t *LFO = (LFO_t *)calloc(1, sizeof(LFO_t));
    LFO_prepare_to_play(LFO, self);
    myDelay->LFO_handle = LFO; //custom
    myDelay->feedback = 0.1f; //custom
    myDelay->oldSample[0] = 0.0f; //custom
    myDelay->oldSample[1] = 0.0f; //custom
    myDelay->debug = 0; //custom da cancellare
    myDelay->max = 0.0f; //custom da cancellare
    myDelay->min = 0.0f; //custom da cancellare
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
    if (myDelay->LFO_handle != NULL) {
        audio_free(myDelay->LFO_handle);
        myDelay->LFO_handle = NULL;
    }

    if(myDelay->delayMemory != NULL){
        heap_caps_free(myDelay->delayMemory); // versione con PSRAM
        myDelay->delayMemory = NULL;
    }  
    myDelay->feedback = 0.0f; //custom
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
    
    int16_t *pbuf16 = (int16_t *)myDelay->buf; //custom
    int16_t *pDelayMem16 = (int16_t *)myDelay->delayMemory; //custom
        
    // DEBUG
    myDelay->feedback = 0.2f;
    // end DEBUG
    float dt = 0.5f; //custom DA CANCELLARE
    // float mod = 0.0f; //custom 
    float dryWetRatio = 0.5f; // custom dry wet mix -> DA ESPORRE

    // stereo version
    for(int i=0; i<r_size / 2; i+=2) { //custom
        float mod = LFO_get_next_sample(myDelay->LFO_handle); //custom
        float current_dt = dt + mod; //custom 
        current_dt = current_dt > MYDELAY_MAX_DELAY_TIME ? MYDELAY_MAX_DELAY_TIME : current_dt; //custom: clamp to max delay time
        for (int ch=0; ch<myDelay->channel; ch++) { //custom
            float inputSample = (float)pbuf16[i+ch] / 32767.0f; //custom
            // DEBUG
            // if (inputSample > myDelay->max) {
            //     myDelay->max = inputSample; 
            // }
            // if (inputSample < myDelay->min) {
            //     myDelay->min = inputSample; 
            // }
            // if (myDelay->debug%100000==0) {
            //     ESP_LOGI(TAG, "myDelay inputSample[%d][%d]: %.3f", i, ch, inputSample);
                // ESP_LOGI(TAG, "channel and myDelay->channel: %d %d", ch, myDelay->channel);
                // ESP_LOGI(TAG, "myDelay dt: %.3f", dt);
                // ESP_LOGI(TAG, "myDelay mod: %.3f", mod);
            //     ESP_LOGI(TAG, "LFO currentValue: %.3f", LFO->currentValue);
            //     ESP_LOGI(TAG, "myDelay MAX inputSample: %.3f", myDelay->max); 
            //     ESP_LOGI(TAG, "myDelay MIN inputSample: %.3f", myDelay->min); 
            //     ESP_LOGI(TAG, "pbuf16[%d]: %d", i, pbuf16[i]);
            // }
            // end DEBUG
            
            //crazy try
            // float realFrac = (float)(myDelay->writeIndex + ch) - (dt * (float)myDelay->samplerate);
            // int integerRealFrac = (int) realFrac;
            // float realFractionalPart = realFrac - integerRealFrac;
            // float readIndex = (float)(myDelay->writeIndex + ch) - (dt * (float)myDelay->samplerate * myDelay->channel); //custom
            // int realIntegerPart = (int)readIndex; //custom: prova anche a fare + ch
            // realIntegerPart = (realIntegerPart%2==0 && ch==0) || (realIntegerPart%2==1 && ch==1) ? realIntegerPart : realIntegerPart - 1; //custom: force even index for left channel
            // float alpha = realFractionalPart / (2.0f - realFractionalPart); //custom

            // int A = (realIntegerPart + myDelay->memorySize) % myDelay->memorySize; //custommmmmmmmmmmmm
            // int B = (A + myDelay->channel) % myDelay->memorySize; //custom
            //end of crazy try

            float readIndex = (float)(myDelay->writeIndex + ch) - (current_dt * (float)myDelay->samplerate * myDelay->channel); //custom
            int integerPart = (int) readIndex; //custom
            float fractionalPart = readIndex - integerPart; //custom
            float alpha = fractionalPart / (2.0f - fractionalPart); //custom
            
            // integerPart = (integerPart%2==0 && ch==0) || (integerPart%2==1 && ch==1) ? integerPart : (ch==0 ? integerPart - 1 : integerPart + 1); //custom: force even index for left channel
            int A = (integerPart + myDelay->memorySize) % myDelay->memorySize; //custommmmmmmmmmmmm
            A = (A%2==0 && ch==0) || (A%2==1 && ch==1) ? A : (ch==0 ? A - 1 : A + 1);
            // int B = (A + 1) % myDelay->memorySize; //custom
            int B = (A + myDelay->channel) % myDelay->memorySize; //custom

            pDelayMem16[myDelay->writeIndex + ch] = pbuf16[i+ch]; //custom
            // if (myDelay->debug%100001==0) {
                // ESP_LOGI(TAG, "my Delay after mydelat memory assignment");
                // ESP_LOGI(TAG, "my Delay pdelayMem16[writeIndex + ch][%d][%d]: %d", i, ch, pDelayMem16[myDelay->writeIndex + ch]);
                // ESP_LOGI(TAG, "my Delay PBUF16[%d][%d]: %d", i, ch, pbuf16[i+ch]);
            // }

            float sample_A_float = (float)pDelayMem16[A] / 32767.0f;
            float sample_B_float = (float)pDelayMem16[B] / 32767.0f;
            float sampleValue = alpha * (sample_B_float - myDelay->oldSample[ch]) + sample_A_float; 
            
            myDelay->oldSample[ch] = sampleValue; //custom
            
            pbuf16[i+ch] = (int16_t)(sampleValue * 32767.0f);

            float delayedSample = inputSample + sampleValue * myDelay->feedback; //custom
            pDelayMem16[myDelay->writeIndex + ch] = (int16_t)(delayedSample * 32767.0f);

            
            // dry wet mix
            float outputSample = sampleValue * sqrtf(1 - dryWetRatio) + inputSample * sqrtf(dryWetRatio);
            // float outputSample = sampleValue * (1 - dryWetRatio) + inputSample * (dryWetRatio); //custom
            
            // DEBUG
            // if (outputSample > myDelay->max) {
            //     myDelay->max = outputSample; 
            // }
            // if (outputSample < myDelay->min) {
            //     myDelay->min = outputSample; 
            // }
            if (myDelay->debug%1000001==0) {
                ESP_LOGI(TAG, "my Delay SPECS");
                ESP_LOGI(TAG, "my Delay inputSample[%d][%d]: %.3f", i, ch, inputSample);
                ESP_LOGI(TAG, "my Delay readIndex[%d][%d]: %.3f", i, ch, readIndex);
                ESP_LOGI(TAG, "my Delay integerPart[%d][%d]: %d", i, ch, integerPart);
                ESP_LOGI(TAG, "my Delay fractionalPart[%d][%d]: %.3f", i, ch, fractionalPart);
                ESP_LOGI(TAG, "my Delay alpha[%d][%d]: %.3f", i, ch, alpha);
                ESP_LOGI(TAG, "my Delay A index[%d][%d]: %d", i, ch, A);
                ESP_LOGI(TAG, "my Delay B index[%d][%d]: %d", i, ch, B);
                ESP_LOGI(TAG, "writeIndex + ch and ch: %d %d", myDelay->writeIndex + ch, ch);
                ESP_LOGI(TAG, "my Delay memorySize: %d", myDelay->memorySize);
                ESP_LOGI(TAG, "myDelay current_dt: %.3f", current_dt);
                

            //     ESP_LOGI(TAG, "myDelay MAX outputSample: %.3f", myDelay->max); 
            //     ESP_LOGI(TAG, "myDelay MIN outputSample: %.3f", myDelay->min); 
            //     ESP_LOGI(TAG, "pbuf16[%d]: %d", i, pbuf16[i]);
            }
            myDelay->debug = myDelay->debug + 1; //custom da cancellare 
            // end DEBUG
            
            pbuf16[i+ch] = (int16_t)(outputSample * 32767.0f);
            
        }
        // myDelay->writeIndex = (myDelay->writeIndex + 1) % myDelay->memorySize; //custom
        myDelay->writeIndex = (myDelay->writeIndex + myDelay->channel) % myDelay->memorySize; //custom
    }
    // end stereo version

    // for(int i=0; i<r_size / 2; i++){ //custom 
        // vers 1

        // float inputSample = (float)pbuf16[i] / 32768.0f; //custom
        // if (inputSample > 1.0f) inputSample = 1.0f; // custom
        // else if (inputSample < -1.0f) inputSample = -1.0f; //custom
        
        // DEBUG
        // if (inputSample > myDelay->max) {
        //     myDelay->max = inputSample; 
        // }
        // if (inputSample < myDelay->min) {
        //     myDelay->min = inputSample; 
        // }
        // if (myDelay->debug%100000==0) {
        //     ESP_LOGI(TAG, "myDelay MAX inputSample: %.3f", myDelay->max); 
        //     ESP_LOGI(TAG, "myDelay MIN inputSample: %.3f", myDelay->min); 
        //     ESP_LOGI(TAG, "pbuf16[%d]: %d", i, pbuf16[i]);
        // }
        // end DEBUG
        
        // LFO_get_next_sample(myDelay->LFO_handle, &dt); //custom
        // dt = fminf(dt, MYDELAY_MAX_DELAY_TIME); //custom: clamp to max delay time
        // float readIndex = (float)myDelay->writeIndex - (dt * (float)myDelay->samplerate) ; //custom
        // int integerPart = (int) readIndex; //custom
        // float fractionalPart = readIndex - integerPart; //custom
        // float alpha = fractionalPart / (2.0f - fractionalPart); //custom
        
        // int A = (integerPart + myDelay->memorySize) % myDelay->memorySize; //custom
        // int B = (A + 1) % myDelay->memorySize; //custom

        // pDelayMem16[myDelay->writeIndex] = pbuf16[i]; //custom

        // float sample_A_float = (float)pDelayMem16[A] / 32768.0f;
        // float sample_B_float = (float)pDelayMem16[B] / 32768.0f;
        // float sampleValue = alpha * (sample_B_float - myDelay->oldSample) + sample_A_float; 
        
        // myDelay->oldSample = sampleValue; //custom
        
        // pbuf16[i] = (int16_t)(sampleValue * 32768.0f);

        // float delayedSample = inputSample + sampleValue * myDelay->feedback; //custom
        // pDelayMem16[myDelay->writeIndex] = (int16_t)(delayedSample * 32768.0f);

        // myDelay->writeIndex = (myDelay->writeIndex + 1) % myDelay->memorySize; //custom

        // // dry wet mix
        // float outputSample = sampleValue * sqrtf(1 - dryWetRatio) + inputSample * sqrtf(dryWetRatio); //custom
        
        // if (outputSample > 1.0f) outputSample = 1.0f; // custom
        // else if (outputSample < -1.0f) outputSample = -1.0f; //custom

        // DEBUG
        // if (outputSample > myDelay->max) {
        //     myDelay->max = outputSample; 
        // }
        // if (outputSample < myDelay->min) {
        //     myDelay->min = outputSample; 
        // }
        // if (myDelay->debug%100000==0) {
        //     ESP_LOGI(TAG, "myDelay MAX outputSample: %.3f", myDelay->max); 
        //     ESP_LOGI(TAG, "myDelay MIN outputSample: %.3f", myDelay->min); 
        //     ESP_LOGI(TAG, "pbuf16[%d]: %d", i, pbuf16[i]);
        // }
        // myDelay->debug = myDelay->debug + 1; //custom da cancellare 
        // end DEBUG
        
        // pbuf16[i] = (int16_t)(outputSample * 32768.0f);
        
        // end vers 1
    // }
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
