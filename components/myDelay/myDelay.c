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

#define BUF_SIZE (512) // custom value

typedef struct LFO {
    int  samplerate;
    int  channel; // number of channels
    float frequency; //custom
    float frequency_target; //custom
    int waveform;  //custom
    float current_phase; //custom
    float sampling_period; //custom
    float mod_amount; //custom
    float mod_amount_target; //custom
    float alpha; //custom
    int debugCount; //DEBUG DA CANCELLARE
} LFO_t;

typedef struct myDelay {
    int  samplerate;
    int  channel; // number of channels
    unsigned char *buf;
    unsigned char *delay_memory; //custom
    int memory_size;     //custom
    float base_dt; //custom
    float base_dt_target; //custom 
    int write_index;   //custom
    float old_sample[2]; //custom
    float feedback;   //custom
    float feedback_target; //custom
    LFO_t *LFO_handle; //custom
    float dw_ratio; //custom
    float dw_ratio_target; //custom
    float alpha; //custom 
    int debug; //custom da cancellare
    float max; //custom da cancellare
    float min; //custom da cancellare
    int  byte_num;
    int  at_eof;
} myDelay_t;

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

esp_err_t myDelay_compute_smoothed_values(myDelay_t *myDelay) {
    // y[n] = y[n-1] + alpha (x[n] - y[n-1])
    // y[n] = alpha * x[n] + (1 - alpha) y[n-1]
    myDelay->base_dt = myDelay->base_dt + myDelay->alpha * (myDelay->base_dt_target - myDelay->base_dt);
    myDelay->feedback = myDelay->feedback + myDelay->alpha * (myDelay->feedback_target - myDelay->feedback);
    myDelay->dw_ratio = myDelay->dw_ratio + myDelay->alpha * (myDelay->dw_ratio_target - myDelay->dw_ratio);
    myDelay->LFO_handle->frequency = myDelay->LFO_handle->frequency + myDelay->LFO_handle->alpha * (myDelay->LFO_handle->frequency_target - myDelay->LFO_handle->frequency);
    myDelay->LFO_handle->mod_amount = myDelay->LFO_handle->mod_amount + myDelay->LFO_handle->alpha * (myDelay->LFO_handle->mod_amount_target - myDelay->LFO_handle->mod_amount);
    return ESP_OK;
}

esp_err_t myDelay_set_base_dt_target(audio_element_handle_t self, float new_base_dt_target) { // STEP 0.0001
    myDelay_t *myDelay = (myDelay_t *)audio_element_getdata(self); 
    if (new_base_dt_target < 0.0001f || new_base_dt_target > MYDELAY_MAX_DELAY_TIME) { 
        ESP_LOGE(TAG, "Base delay time must be between 0.001 and %.4f seconds. (line %d)", MYDELAY_MAX_DELAY_TIME, __LINE__);
        return ESP_ERR_INVALID_ARG;
    }
    myDelay->base_dt_target = new_base_dt_target;
    ESP_LOGI(TAG, "Base delay time target set to %.4f seconds", new_base_dt_target);
    return ESP_OK;
}

esp_err_t myDelay_set_feedback(audio_element_handle_t self, float new_feedback) {   // STEP 0.001
    myDelay_t *myDelay = (myDelay_t *)audio_element_getdata(self);
    if (new_feedback < 0.000f || new_feedback > 0.999f) { 
        ESP_LOGE(TAG, "Feedback must be between 0 and 0.999. (line %d)", __LINE__);
        return ESP_ERR_INVALID_ARG;
    }
    myDelay->feedback_target = new_feedback;
    ESP_LOGI(TAG, "Feedback set to %.3f", new_feedback);
    return ESP_OK;
}

esp_err_t myDelay_set_dw_ratio(audio_element_handle_t self, float new_dw_ratio) {   // STEP 0.001
    myDelay_t *myDelay = (myDelay_t *)audio_element_getdata(self);
    if (new_dw_ratio < 0.000f || new_dw_ratio > 1.000f) {
        ESP_LOGE(TAG, "Dry/Wet ratio must be between 0.0 and 1.0. (line %d)", __LINE__);
        return ESP_ERR_INVALID_ARG;
    }
    myDelay->dw_ratio_target = new_dw_ratio;
    ESP_LOGI(TAG, "Dry/Wet ratio set to %.4f", new_dw_ratio);
    return ESP_OK;
}

float myDelay_get_base_dt_target(audio_element_handle_t self) {
    myDelay_t *myDelay = (myDelay_t *)audio_element_getdata(self);
    return myDelay->base_dt;
}

float myDelay_get_feedback(audio_element_handle_t self) {
    myDelay_t *myDelay = (myDelay_t *)audio_element_getdata(self);
    return myDelay->feedback;
}

float myDelay_get_dw_ratio(audio_element_handle_t self) {
    myDelay_t *myDelay = (myDelay_t *)audio_element_getdata(self);
    return myDelay->dw_ratio;
}

// LFO
esp_err_t myDelay_set_LFO_frequency(audio_element_handle_t self, float new_frequency) { // STEP 0.01
    myDelay_t *myDelay = (myDelay_t *)audio_element_getdata(self);
    if (new_frequency < 0.01f || new_frequency > 20.00f) { 
        ESP_LOGE(TAG, "LFO frequency must be between 0.01 and 20.0 Hz. (line %d)", __LINE__); 
        return ESP_ERR_INVALID_ARG;
    }
    myDelay->LFO_handle->frequency_target = new_frequency;
    ESP_LOGI(TAG, "LFO frequency set to %.2f Hz", new_frequency);
    return ESP_OK;
}

esp_err_t myDelay_set_LFO_waveform(audio_element_handle_t self, int new_waveform) {
    myDelay_t *myDelay = (myDelay_t *)audio_element_getdata(self);
    if (new_waveform < 0 || new_waveform > 3) { // check boundaries
        ESP_LOGE(TAG, "LFO waveform must be between 0 and 3. (line %d)", __LINE__); //check error message
        return ESP_ERR_INVALID_ARG;
    }
    myDelay->LFO_handle->waveform = new_waveform;
    ESP_LOGI(TAG, "LFO waveform set to %d", new_waveform);
    return ESP_OK;
}

esp_err_t myDelay_set_LFO_mod_amount(audio_element_handle_t self, float new_mod_amount) { // STEP 0.001 (in sec)
    myDelay_t *myDelay = (myDelay_t *)audio_element_getdata(self);
    if (new_mod_amount < 0.001f || new_mod_amount > 1.000f) { 
        ESP_LOGE(TAG, "LFO modulation amount must be between 0.001 and 1 (seconds). (line %d)", __LINE__); 
        return ESP_ERR_INVALID_ARG;
    }
    myDelay->LFO_handle->mod_amount_target = new_mod_amount;
    ESP_LOGI(TAG, "LFO modulation amount set to %.3f", new_mod_amount);
    return ESP_OK;
}

float myDelay_get_LFO_frequency(audio_element_handle_t self) {
    myDelay_t *myDelay = (myDelay_t *)audio_element_getdata(self);
    return myDelay->LFO_handle->frequency;
}

int myDelay_get_LFO_waveform(audio_element_handle_t self) {
    myDelay_t *myDelay = (myDelay_t *)audio_element_getdata(self);
    return myDelay->LFO_handle->waveform;
}

float myDelay_get_LFO_mod_amount(audio_element_handle_t self) {
    myDelay_t *myDelay = (myDelay_t *)audio_element_getdata(self);
    return myDelay->LFO_handle->mod_amount;
}

esp_err_t LFO_prepare_to_play(LFO_t *LFO, audio_element_handle_t my_d) 
{
    myDelay_t *myDelay = (myDelay_t *)audio_element_getdata(my_d);
    LFO->samplerate = myDelay->samplerate;
    LFO->current_phase = 0.0f; 
    LFO->sampling_period = 1.0f / (float)LFO->samplerate;
    LFO->frequency = 1.0f; //custom
    LFO->waveform = 1;  //custom: triangle wave
    LFO->mod_amount = 0.01f; //custom: modulation amount
    LFO->alpha = 1.0f - expf(-1.0f/(0.3f * (float)myDelay->samplerate)); //custom 
    LFO->frequency_target = LFO->frequency; //custom
    LFO->mod_amount_target = LFO->mod_amount; //custom
    LFO->debugCount = 0; //DEBUG DA CANCELLARE
    ESP_LOGI(TAG, "LFO setup completed.");
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
    float sample = 0.0f;
    switch (LFO->waveform) {
        case 0: // Sine wave
            sample = sinf(LFO->current_phase * 2.0f * M_PI);
            break;
        case 1: // Triangle wave
            sample = 4.0f * fabsf(LFO->current_phase - 0.5f) - 1.0f; //check
            break;
        default:
            sample = 0.0f; // Default to silence for unknown waveform types
            break;
    }
    
    float phaseIncrement = LFO->frequency * LFO->sampling_period;
    LFO->current_phase += phaseIncrement;
    LFO->current_phase -= (int)LFO->current_phase;
    get_unipolar_LFO(&sample); // Scale to [0, 1] range
    sample = sample * LFO->mod_amount; // Scale by modulation amount
    // if (LFO->debugCount%100000 == 0) {
        // ESP_LOGI(TAG, "LFO frequency: %.2f Hz", LFO->frequency);
        // ESP_LOGI(TAG, "LFO waveform type: %d", LFO->waveform);  
        // ESP_LOGI(TAG, "LFO phaseincrement: %.6f", phaseIncrement);
        // ESP_LOGI(TAG, "LFO current_phase: %.6f", LFO->current_phase);
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

    myDelay->memory_size = (int)(MYDELAY_MAX_DELAY_TIME * myDelay->samplerate)  + BUF_SIZE / sizeof(int16_t) ; //custom: è in campioni
    myDelay->memory_size = myDelay->memory_size * myDelay->channel; //custom!!! la raddoppio se sono in stereo perché ho i canali interleaved
    
    //versione con PSRAM
    size_t delay_bytes = (size_t)myDelay->memory_size * sizeof(int16_t); //custom: è in byte
    ESP_LOGI(TAG, "myDelay memory size in bytes: %u", (unsigned)delay_bytes);

    // Sostituito calloc() con heap_caps_calloc() e il flag MALLOC_CAP_SPIRAM
    myDelay->delay_memory = (unsigned char *)heap_caps_calloc(1, delay_bytes, MALLOC_CAP_SPIRAM); 
    if (myDelay->delay_memory == NULL) {
        ESP_LOGE(TAG, "calloc FAILED! Could not allocate %u bytes in PSRAM.", delay_bytes);
        audio_free(myDelay->buf); 
        myDelay->buf = NULL;
        return ESP_ERR_NO_MEM;
    }
    memset(myDelay->delay_memory, 0, delay_bytes);
    // end versione con PSRAM

    LFO_t *LFO = (LFO_t *)calloc(1, sizeof(LFO_t));
    LFO_prepare_to_play(LFO, self);
    myDelay->LFO_handle = LFO; //custom
    myDelay->base_dt = 0.08f; //custom
    myDelay->write_index = 0; //custom
    myDelay->feedback = 0.1f; //custom
    myDelay->old_sample[0] = 0.0f; //custom
    myDelay->old_sample[1] = 0.0f; //custom
    myDelay->dw_ratio = 0.5f; //custom
    myDelay->base_dt_target = myDelay->base_dt; //custom
    myDelay->feedback_target = myDelay->feedback; //custom
    myDelay->dw_ratio_target = myDelay->dw_ratio; //custom
    myDelay->alpha = 1.0f - expf(-1.0f/(0.3f * (float)myDelay->samplerate)); //custom da cancellare
    ESP_LOGI(TAG, "Smoothing alpha: %.6f", myDelay->alpha);
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

    if(myDelay->delay_memory != NULL){
        heap_caps_free(myDelay->delay_memory); // versione con PSRAM
        myDelay->delay_memory = NULL;
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
    if (r_size % (myDelay->channel * sizeof(int16_t)) != 0) { 
        ESP_LOGW(TAG, "Input data size %d is not aligned with channel number %d and sample size %d.", r_size, myDelay->channel, sizeof(int16_t)); //check error message
    }

    float num_samples = (float)r_size / (float)sizeof(int16_t); //custom
    
    int16_t *p_buf_16 = (int16_t *)myDelay->buf; //custom
    int16_t *p_delay_memory_16 = (int16_t *)myDelay->delay_memory; //custom

    // stereo version
    for(int i = 0; i < num_samples; i += 2) { //custom
        myDelay_compute_smoothed_values(myDelay); //custom
        float mod = LFO_get_next_sample(myDelay->LFO_handle); //custom
        float current_dt = myDelay->base_dt + mod; //custom 
        current_dt = current_dt > MYDELAY_MAX_DELAY_TIME ? MYDELAY_MAX_DELAY_TIME : current_dt; //custom: clamp to max delay time
        for (int ch = 0; ch < myDelay->channel; ch++) { //custom
            float input_sample = (float)p_buf_16[i + ch] / 32767.0f; //custom
            // DEBUG
            // if (input_sample > myDelay->max) {
            //     myDelay->max = input_sample; 
            // }
            // if (input_sample < myDelay->min) {
            //     myDelay->min = input_sample; 
            // }
            // if (myDelay->debug%100000==0) {
            //     ESP_LOGI(TAG, "myDelay input_sample[%d][%d]: %.3f", i, ch, input_sample);
                // ESP_LOGI(TAG, "channel and myDelay->channel: %d %d", ch, myDelay->channel);
                // ESP_LOGI(TAG, "myDelay dt: %.3f", dt);
                // ESP_LOGI(TAG, "myDelay mod: %.3f", mod);
            //     ESP_LOGI(TAG, "LFO currentValue: %.3f", LFO->currentValue);
            //     ESP_LOGI(TAG, "myDelay MAX input_sample: %.3f", myDelay->max); 
            //     ESP_LOGI(TAG, "myDelay MIN input_sample: %.3f", myDelay->min); 
            //     ESP_LOGI(TAG, "p_buf_16[%d]: %d", i, p_buf_16[i]);
            // }
            // end DEBUG

            float read_index = (float)(myDelay->write_index + ch) - (current_dt * (float)myDelay->samplerate * myDelay->channel); //custom
            int integer_part = (int) read_index; //custom
            float fractional_part = read_index - integer_part; //custom
            float alpha = fractional_part / (2.0f - fractional_part); //custom
            
            int A = (integer_part + myDelay->memory_size) % myDelay->memory_size; //custom
            A = (A % 2 == 0 && ch == 0) || (A % 2 == 1 && ch == 1) ? A : (ch == 0 ? A - 1 : A + 1);
            int B = (A + myDelay->channel) % myDelay->memory_size; //custom

            p_delay_memory_16[myDelay->write_index + ch] = p_buf_16[i + ch]; //custom
            // if (myDelay->debug%100001==0) {
                // ESP_LOGI(TAG, "my Delay after mydelat memory assignment");
                // ESP_LOGI(TAG, "my Delay p_delay_memory_16[write_index + ch][%d][%d]: %d", i, ch, p_delay_memory_16[myDelay->write_index + ch]);
                // ESP_LOGI(TAG, "my Delay p_buf_16[%d][%d]: %d", i, ch, p_buf_16[i+ch]);
            // }

            float sample_A_float = (float)p_delay_memory_16[A] / 32767.0f;
            float sample_B_float = (float)p_delay_memory_16[B] / 32767.0f;
            float sample_value = alpha * (sample_B_float - myDelay->old_sample[ch]) + sample_A_float; 
            
            myDelay->old_sample[ch] = sample_value; //custom
            
            p_buf_16[i + ch] = (int16_t)(sample_value * 32767.0f);
            
            // feedback
            float delayed_sample = input_sample + sample_value * myDelay->feedback; //custom
            p_delay_memory_16[myDelay->write_index + ch] = (int16_t)(delayed_sample * 32767.0f);

            // dry wet mix
            // float output_sample = sample_value * sqrtf(1.0f - myDelay->dw_ratio) + input_sample * sqrtf(myDelay->dw_ratio);
            float output_sample = (roundf(myDelay->dw_ratio * 1000.0f) / 1000.0f == 1.0f) || (roundf(myDelay->dw_ratio * 1000.0f) / 1000.0f == 0.0f) ? (roundf(myDelay->dw_ratio * 1000.0f) / 1000.0f == 1.0f ? input_sample : sample_value) : sample_value * sqrtf(1.0f - myDelay->dw_ratio) + input_sample * sqrtf(myDelay->dw_ratio); //bypass when dw_ratio is 1.0
            
            // DEBUG
            if (output_sample > myDelay->max) {
                myDelay->max = output_sample; 
            }
            if (output_sample < myDelay->min) {
                myDelay->min = output_sample; 
            }
            if (myDelay->debug%1000001==0) {
                ESP_LOGI(TAG, "my Delay SPECS");
                // ESP_LOGI(TAG, "my Delay input_sample[%d][%d]: %.3f", i, ch, input_sample);
                // ESP_LOGI(TAG, "my Delay read_index[%d][%d]: %.3f", i, ch, read_index);
                // ESP_LOGI(TAG, "my Delay integer_part[%d][%d]: %d", i, ch, integer_part);
                // ESP_LOGI(TAG, "my Delay fractional_part[%d][%d]: %.3f", i, ch, fractional_part);
                // ESP_LOGI(TAG, "my Delay alpha[%d][%d]: %.3f", i, ch, alpha);
                // ESP_LOGI(TAG, "my Delay A index[%d][%d]: %d", i, ch, A);
                // ESP_LOGI(TAG, "my Delay B index[%d][%d]: %d", i, ch, B);
                // ESP_LOGI(TAG, "write_index + ch and ch: %d %d", myDelay->write_index + ch, ch);
                // ESP_LOGI(TAG, "my Delay memory_size: %d", myDelay->memory_size);
                ESP_LOGI(TAG, "my Delay feedback: %.3f", myDelay->feedback);
                ESP_LOGI(TAG, "myDelay current_dt: %.3f", current_dt);
                ESP_LOGI(TAG, "my Delay base dt: %.3f", myDelay->base_dt);
                ESP_LOGI(TAG, "DRY WET RATIO: %.3f", myDelay->dw_ratio);
                ESP_LOGI(TAG, "SQRT DW: sqrtf(1.0f - myDelay->dw_ratio) : %.4f e sqrtf(myDelay->dw_ratio) : %.4f", sqrt(1.0f - myDelay->dw_ratio), sqrt(myDelay->dw_ratio));
                ESP_LOGI(TAG, "myDelay MAX output_sample: %.3f", myDelay->max); 
                ESP_LOGI(TAG, "myDelay MIN output_sample: %.3f", myDelay->min); 
            //     ESP_LOGI(TAG, "p_buf_16[%d]: %d", i, p_buf_16[i]);
            }
            myDelay->debug = myDelay->debug + 1; //custom da cancellare 
            // end DEBUG
            
            p_buf_16[i+ch] = (int16_t)(output_sample * 32767.0f);
            
        }
        myDelay->write_index = (myDelay->write_index + myDelay->channel) % myDelay->memory_size; //custom
    }
    // end stereo version
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
