#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "audio_pipeline.h"
#include "i2s_stream.h"
#include "board.h"
#include "myDelay.h"
#include "LFO.h" //custom


static const char *TAG = "MYDELAYPROJECT";

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

    // audio_hal_set_volume(board_handle->audio_hal, 100);   // volume 0–100%
    audio_hal_codec_iface_config(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, &iface);
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
    audio_hal_enable_pa(board_handle->audio_hal, true); 

    ESP_LOGI(TAG, "[ 2 ] Create audio pipeline for playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);

    ESP_LOGI(TAG, "[3.1] Create i2s stream to write data to codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_cfg.std_cfg.clk_cfg.sample_rate_hz = 48000;
    // i2s_cfg.std_cfg.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_24BIT;
    // i2s_cfg.std_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT;
    // i2s_cfg.std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;
    // i2s_cfg.std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_RIGHT;
    i2s_cfg.chan_cfg.role = I2S_ROLE_MASTER; 
    // i2s_cfg.std_cfg.slot_cfg.ws_pol = false;
    // i2s_cfg.chan_cfg.dma_desc_num = 3;
    // i2s_cfg.chan_cfg.dma_frame_num = 312;
    // i2s_cfg.std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_384;
    // i2s_cfg.volume = 100; 
    i2s_stream_writer = i2s_stream_init(&i2s_cfg);
    ESP_LOGI(TAG, "W sample rate in i2s_stream_writer:%d", (int) i2s_cfg.std_cfg.clk_cfg.sample_rate_hz);
    ESP_LOGI(TAG, "W data_bit_width in i2s_stream_writer:%d", (int) i2s_cfg.std_cfg.slot_cfg.data_bit_width);
    ESP_LOGI(TAG, "W slot_mode in i2s_stream_writer:%d", (int) i2s_cfg.std_cfg.slot_cfg.slot_mode);
        

    ESP_LOGI(TAG, "[3.2] Create i2s stream to read data from codec chip");
    i2s_stream_cfg_t i2s_cfg_read = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg_read.type = AUDIO_STREAM_READER;
    i2s_cfg_read.std_cfg.clk_cfg.sample_rate_hz = 48000;
    // i2s_cfg_read.std_cfg.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_24BIT;
    // i2s_cfg_read.std_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT;
    // i2s_cfg_read.std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;
    // i2s_cfg_read.std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_RIGHT;
    i2s_cfg_read.chan_cfg.role = I2S_ROLE_MASTER; 
    // i2s_cfg_read.std_cfg.slot_cfg.ws_pol = false;
    // i2s_cfg_read.chan_cfg.dma_desc_num = 3;
    // i2s_cfg_read.chan_cfg.dma_frame_num = 312;
    // i2s_cfg_read.std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_384;
    // i2s_cfg_read.volume = 100;
    i2s_stream_reader = i2s_stream_init(&i2s_cfg_read);
    ESP_LOGI(TAG, "R sample rate in i2s_stream_reader:%d", (int) i2s_cfg_read.std_cfg.clk_cfg.sample_rate_hz);
    ESP_LOGI(TAG, "R data_bit_width in i2s_stream_reader:%d", (int) i2s_cfg_read.std_cfg.slot_cfg.data_bit_width);
    ESP_LOGI(TAG, "R slot_mode in i2s_stream_reader:%d", (int) i2s_cfg_read.std_cfg.slot_cfg.slot_mode);

    LFO_cfg_t lfo_cfg = DEFAULT_LFO_CONFIG();
    audio_element_handle_t lfo_handle = LFO_init(&lfo_cfg);

    myDelay_cfg_t myDelay_cfg = DEFAULT_MYDELAY_CONFIG();
    delay = myDelay_init(&myDelay_cfg);
    myDelay_set_LFO_handle(delay, lfo_handle); // custom: set LFO handle to myDelay

    ESP_LOGI(TAG, "[3.3] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, i2s_stream_reader, "i2s_read");
    audio_pipeline_register(pipeline, delay, "delay");
    audio_pipeline_register(pipeline, i2s_stream_writer, "i2s_write");

    ESP_LOGI(TAG, "[3.4] Link it together [codec_chip]-->i2s_stream_reader-->i2s_stream_writer-->[codec_chip]");
    const char *link_tag[3] = {"i2s_read", "delay", "i2s_write"};
    audio_pipeline_link(pipeline, &link_tag[0], 3);

    ESP_LOGI(TAG, "[ 4 ] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[4.1] Listening event from all elements of pipeline");
    audio_pipeline_set_listener(pipeline, evt);

    i2s_stream_set_clk(i2s_stream_reader, 48000, 16, 2); // CHECKK THIS 
    i2s_stream_set_clk(i2s_stream_writer, 48000, 16, 2); /// CHECKK THIS
    // ESP_LOGI(TAG, "AFTEREXE sample rate in i2s_stream_writer:%d", (int) i2s_cfg.std_cfg.clk_cfg.sample_rate_hz);
    // ESP_LOGI(TAG, "AFTEREXE data_bit_width in i2s_stream_writer:%d", (int) i2s_cfg.std_cfg.slot_cfg.data_bit_width);
    // ESP_LOGI(TAG, "AFTEREXE slot_mode in i2s_stream_writer:%d", (int) i2s_cfg.std_cfg.slot_cfg.slot_mode);

    ESP_LOGI(TAG, "[ 5 ] Start audio_pipeline");
    audio_pipeline_run(pipeline);

    ESP_LOGI(TAG, "[ 6 ] Listen for all pipeline events");
    while (1) {
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }

        /* Stop when the last pipeline element (i2s_stream_writer in this case) receives stop event */
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) i2s_stream_writer
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS
            && (((int)msg.data == AEL_STATUS_STATE_STOPPED) || ((int)msg.data == AEL_STATUS_STATE_FINISHED))) {
            ESP_LOGW(TAG, "[ * ] Stop event received");
            break;
        }
    }

    ESP_LOGI(TAG, "[ 7 ] Stop audio_pipeline");
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
