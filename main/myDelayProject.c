#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "audio_pipeline.h"
#include "i2s_stream.h"
#include "board.h"
#include "myDelay.h"

#include "esp_peripherals.h" // CHECKKKKKKKKKKK
#include "periph_touch.h" // CHECKKKKKKKKKKK
#include "periph_adc_button.h" // CHECKKKKKKKKKKK
#include "periph_button.h" // CHECKKKKKKKKKKK

// #include "esp_wifi.h"   //custom
#include "esp_adc/adc_oneshot.h"        //custom

//adc
#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/semphr.h"

#include <stdint.h>
#include <sys/unistd.h>
#include "driver/adc_types_legacy.h"
#include "esp_err.h"
#include "freertos/FreeRTOSConfig_arch.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "hal/adc_types.h"
#include "hal/touch_sensor_types.h"
#include "portmacro.h"
#include "soc/soc_caps.h"

// third try
static int adc_raw[2][10];
static int voltage[2][10];
static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);
static void example_adc_calibration_deinit(adc_cali_handle_t handle);
// end third try

//end adc


static const char *TAG = "MYDELAYPROJECT";

// adc
// third try
/*---------------------------------------------------------------
        ADC Calibration
---------------------------------------------------------------*/
static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

static void example_adc_calibration_deinit(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Curve Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));

#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Line Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif
}
// end third try

//end adc

void app_main(void)
{
    audio_pipeline_handle_t pipeline;
    audio_element_handle_t i2s_stream_writer, i2s_stream_reader, delay;
    
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    ESP_LOGI(TAG, "[ 1 ] Start codec chip");
    audio_board_handle_t board_handle = audio_board_init();
    
    audio_hal_codec_i2s_iface_t iface = {
        .mode = AUDIO_HAL_MODE_SLAVE,
        .fmt = AUDIO_HAL_I2S_NORMAL,
        .samples = AUDIO_HAL_48K_SAMPLES,
        .bits = AUDIO_HAL_BIT_LENGTH_16BITS,
    };     

    audio_hal_set_volume(board_handle->audio_hal, 80);
    audio_hal_codec_iface_config(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, &iface);
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
    audio_hal_enable_pa(board_handle->audio_hal, true); 

    int player_volume;
    audio_hal_get_volume(board_handle->audio_hal, &player_volume);

    ESP_LOGI(TAG, "[ 2 ] Create audio pipeline for playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);

    ESP_LOGI(TAG, "[3.1] Create i2s stream to write data to codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_cfg.std_cfg.clk_cfg.sample_rate_hz = 48000;
    i2s_cfg.chan_cfg.role = I2S_ROLE_MASTER;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg);

    ESP_LOGI(TAG, "[3.2] Create i2s stream to read data from codec chip");
    i2s_stream_cfg_t i2s_cfg_read = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg_read.type = AUDIO_STREAM_READER;
    i2s_cfg_read.std_cfg.clk_cfg.sample_rate_hz = 48000;
    i2s_cfg_read.chan_cfg.role = I2S_ROLE_MASTER; 
    i2s_stream_reader = i2s_stream_init(&i2s_cfg_read);

    myDelay_cfg_t myDelay_cfg = DEFAULT_MYDELAY_CONFIG();
    delay = myDelay_init(&myDelay_cfg);

    ESP_LOGI(TAG, "[3.3] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, i2s_stream_reader, "i2s_read");
    audio_pipeline_register(pipeline, delay, "delay");
    audio_pipeline_register(pipeline, i2s_stream_writer, "i2s_write");

    ESP_LOGI(TAG, "[3.4] Link it together [codec_chip]-->i2s_stream_reader-->delay-->i2s_stream_writer-->[codec_chip]");
    const char *link_tag[3] = {"i2s_read", "delay", "i2s_write"};
    audio_pipeline_link(pipeline, &link_tag[0], 3);

    // peripherals
    ESP_LOGI(TAG, "[ 4 ] Initialize peripherals"); 
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "[ 5 ] Initialize keys on board");
    audio_board_key_init(set);
    //end peripherals

    ESP_LOGI(TAG, "[ 6 ] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[6.1] Listening event from all elements of pipeline");
    audio_pipeline_set_listener(pipeline, evt);

    // perip
    ESP_LOGI(TAG, "[6.2] Listening event from peripherals"); // CHECKKKK
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);
    // end perip

    i2s_stream_set_clk(i2s_stream_reader, 48000, 16, 2); // CHECKK THIS 
    i2s_stream_set_clk(i2s_stream_writer, 48000, 16, 2); /// CHECKK THIS
    
    // wifi check
    
    // esp_wifi_disconnect();
    // esp_wifi_stop();
    // esp_wifi_deinit();
    
    // wifi_mode_t mode;
    // esp_err_t err = esp_wifi_get_mode(&mode);

    // if (err == ESP_OK) {
    //     ESP_LOGI(TAG, "err = esp ok");
    // } else if (err == ESP_ERR_WIFI_NOT_INIT) {
    //     ESP_LOGI(TAG, "err = ESP_ERR_WIFI_NOT_INIT");
    // }

    // end wifi check
    
    // adc 
    // third try
    //-------------ADC2 Init---------------//
    adc_oneshot_unit_handle_t adc2_handle;
    adc_oneshot_unit_init_cfg_t init_config2 = {
        .unit_id = ADC_UNIT_2,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config2, &adc2_handle));

    //-------------ADC2 Calibration Init---------------//
    adc_cali_handle_t adc2_cali_handle = NULL;
    bool do_calibration2 = example_adc_calibration_init(ADC_UNIT_2, ADC_CHANNEL_6, ADC_ATTEN_DB_12, &adc2_cali_handle);

    //-------------ADC2 Config---------------//
    // custom
    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    //custom
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc2_handle, ADC_CHANNEL_6, &config));
    // end third try
    // end adc
    
    ESP_LOGI(TAG, "[ 7 ] Start audio_pipeline");
    audio_pipeline_run(pipeline);

    // testing
    float base_dt = myDelay_get_base_dt_target(delay);
    int wf = myDelay_get_LFO_waveform(delay);
    float fb = myDelay_get_feedback(delay);
    float dw = myDelay_get_dw_ratio(delay);
    float lfo_freq = myDelay_get_LFO_frequency(delay);
    float lfo_mod_amount = myDelay_get_LFO_mod_amount(delay);
    int count = 0; //custom

    // end testing
    
    ESP_LOGI(TAG, "[ 8 ] Listen for all pipeline events");

    while (1) {
        audio_event_iface_msg_t msg;
        // esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY); 
        esp_err_t ret = audio_event_iface_listen(evt, &msg, 1000 / portTICK_RATE_MS); // custom
        // if (ret != ESP_OK) {
        //     ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
        //     continue;
        // }

        // buttons management
        if ((msg.source_type == PERIPH_ID_TOUCH || msg.source_type == PERIPH_ID_BUTTON || msg.source_type == PERIPH_ID_ADC_BTN)
            && (msg.cmd == PERIPH_TOUCH_TAP || msg.cmd == PERIPH_BUTTON_PRESSED || msg.cmd == PERIPH_ADC_BUTTON_PRESSED)) {
            if ((int) msg.data == get_input_mode_id()) { // key 1
                base_dt += 0.5f;
                base_dt = roundf(base_dt * 10000.0f) / 10000.0f;
                base_dt = base_dt > MYDELAY_MAX_DELAY_TIME ? 0.0001f : base_dt;
                esp_err_t ret = myDelay_set_base_dt_target(delay, base_dt); // example
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "[ * ] Changed base delay time target to %.4f seconds", base_dt);
                }
            } else if ((int) msg.data == get_input_rec_id()) { // key 2
                fb += 0.05f;
                fb = roundf(fb * 1000.0f) / 1000.0f;
                fb = fb > 0.999f ? 0.0f : fb;
                esp_err_t ret = myDelay_set_feedback(delay, fb); // example
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "[ * ] Changed feedback to %.3f", fb);
                }
            } else if ((int) msg.data == get_input_play_id()) { // key 3
                wf = (wf + 1) % 2;
                esp_err_t ret = myDelay_set_LFO_waveform(delay, wf); // example
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "[ * ] Changed LFO waveform to %d", wf);
                }
            } else if ((int) msg.data == get_input_set_id()) { // key 4
                lfo_freq += 0.5f;
                lfo_freq = roundf(lfo_freq * 100.0f) / 100.0f;
                lfo_freq = lfo_freq > 20.0f ? 0.01f : lfo_freq;
                esp_err_t ret = myDelay_set_LFO_frequency(delay, lfo_freq); // example
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "[ * ] Changed LFO frequency to %.3f Hz", lfo_freq);
                }
            } else if ((int) msg.data == get_input_voldown_id()) { // key 5
                lfo_mod_amount += 0.01f;
                lfo_mod_amount = roundf(lfo_mod_amount * 1000.0f) / 1000.0f;
                lfo_mod_amount = lfo_mod_amount > 1.0f ? 0.001f : lfo_mod_amount;
                esp_err_t ret = myDelay_set_LFO_mod_amount(delay, lfo_mod_amount); // example
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "[ * ] Changed LFO modulation amount to %.3f", lfo_mod_amount);
                }
            } else if ((int) msg.data == get_input_volup_id()) { // key 6
                // player_volume = (player_volume + 10) % 100;
                // audio_hal_set_volume(board_handle->audio_hal, player_volume);
                // ESP_LOGI(TAG, "[ * ] Volume set to %d %%", player_volume);
                dw += 0.1f;
                dw = roundf(dw * 1000.0f) / 1000.0f;
                dw = dw > 1.0f ? 0.0f : dw;
                esp_err_t ret = myDelay_set_dw_ratio(delay, dw); // example
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "[ * ] Changed dry/wet ratio to %.4f", dw);
                }
            } 
    
        } else {
            //adc
            // third try
            ESP_LOGI(TAG, "%s...................................................ADC2 UNIT READING...................................................%s", "\033[35m","\033[35m");
            ESP_ERROR_CHECK(adc_oneshot_read(adc2_handle, ADC_CHANNEL_6, &adc_raw[1][0]));
            ESP_LOGI(TAG, "%sADC%d Channel[%d] Raw Data: %d %s", "\033[35m", ADC_UNIT_2 + 1, ADC_CHANNEL_6, adc_raw[1][0], "\033[35m");
            if (do_calibration2) {
                ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc2_cali_handle, adc_raw[1][0], &voltage[1][0]));
                ESP_LOGI(TAG, "%sADC%d Channel[%d] Cali Voltage: %d mV %s", "\033[35m", ADC_UNIT_2 + 1, ADC_CHANNEL_6, voltage[1][0], "\033[35m");
            }
            ESP_LOGI(TAG, "%s.......................................................................................................................%s", "\033[35m","\033[35m");
            continue;
            // end third try
            // end adc
        }
            
        // end of buttons management 

        /* Stop when the last pipeline element (i2s_stream_writer in this case) receives stop event */
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) i2s_stream_writer
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS
            && (((int)msg.data == AEL_STATUS_STATE_STOPPED) || ((int)msg.data == AEL_STATUS_STATE_FINISHED))) {
            ESP_LOGW(TAG, "[ * ] Stop event received");
            break;
        }
        // count++; // custom
    }

    // adc
    // third try
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc2_handle));
    if (do_calibration2) {
        example_adc_calibration_deinit(adc2_cali_handle);
    }
    // end third try
    // end adc

    ESP_LOGI(TAG, "[ 9 ] Stop audio_pipeline");
    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);

    audio_pipeline_unregister(pipeline, i2s_stream_reader);
    audio_pipeline_unregister(pipeline, delay);
    audio_pipeline_unregister(pipeline, i2s_stream_writer);

    /* Terminate the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);

    /* Release all resources */
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(i2s_stream_reader);
    audio_element_deinit(delay);
    audio_element_deinit(i2s_stream_writer);

}

