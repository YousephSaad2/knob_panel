/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai)
 * CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_spiffs.h"
#include "esp_vfs.h"

#include "app_audio.h"
#include "audio_player.h"
#include "bsp/esp-bsp.h"

static const char* TAG = "app_audio";

static esp_codec_dev_handle_t play_dev_handle;

static esp_err_t bsp_audio_reconfig_clk(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch);
static esp_err_t bsp_audio_write(void* audio_buffer, size_t len, size_t* bytes_written, uint32_t timeout_ms);

esp_err_t audio_force_quite(bool ret)
{
    return audio_player_stop();
}

esp_err_t app_audio_write(void* audio_buffer, size_t len, size_t* bytes_written, uint32_t timeout_ms)
{
    esp_err_t ret = ESP_OK;

    if (bsp_audio_write(audio_buffer, len, bytes_written, 1000) != ESP_OK) {
        ESP_LOGE(TAG, "Write Task: i2s write failed");
        ret = ESP_FAIL;
    }

    return ret;
}

esp_err_t audio_handle_info(PDM_SOUND_TYPE voice)
{
    char filepath[30];
    esp_err_t ret = ESP_OK;

    switch (voice) {
    case SOUND_TYPE_KNOB:
        sprintf(filepath, "%s/%s", CONFIG_BSP_SPIFFS_MOUNT_POINT, "brightness_100.mp3");
        break;
    case SOUND_TYPE_SNORE:
        sprintf(filepath, "%s/%s", CONFIG_BSP_SPIFFS_MOUNT_POINT, "brightness_75.mp3");
        break;
    case SOUND_TYPE_WASH_END_CN:
        sprintf(filepath, "%s/%s", CONFIG_BSP_SPIFFS_MOUNT_POINT, "brightness_50.mp3");
        break;
    case SOUND_TYPE_WASH_END_EN:
        sprintf(filepath, "%s/%s", CONFIG_BSP_SPIFFS_MOUNT_POINT, "brightness_25.mp3");
        break;
    case SOUND_TYPE_FACTORY:
        sprintf(filepath, "%s/%s", CONFIG_BSP_SPIFFS_MOUNT_POINT, "brightness_0.mp3");
        break;

        // Add cases for the missing enum values
    case SOUND_TYPE_BRIGHTNESS_0:
    case SOUND_TYPE_BRIGHTNESS_25:
    case SOUND_TYPE_BRIGHTNESS_50:
    case SOUND_TYPE_BRIGHTNESS_75:
    case SOUND_TYPE_BRIGHTNESS_100:
        ESP_LOGW(TAG, "Unhandled sound type: %d", voice);
        ret = ESP_ERR_INVALID_ARG;
        goto err;
        break;

    default:
        ESP_LOGE(TAG, "Unknown sound type: %d", voice);
        ret = ESP_ERR_INVALID_ARG;
        goto err;
    }

    FILE* fp = fopen(filepath, "r");
    ESP_GOTO_ON_FALSE(fp, ESP_FAIL, err, TAG, "Failed to open file: %s", filepath);

    ESP_LOGI(TAG, "play: %s", filepath);
    ret = audio_player_play(fp);

err:
    return ret;
}

static esp_err_t app_mute_function(AUDIO_PLAYER_MUTE_SETTING setting)
{
    return ESP_OK;
}

static void audio_callback(audio_player_cb_ctx_t* ctx)
{
    switch (ctx->audio_event) {
    case AUDIO_PLAYER_CALLBACK_EVENT_IDLE:
        ESP_LOGI(TAG, "IDLE");
        break;
    case AUDIO_PLAYER_CALLBACK_EVENT_COMPLETED_PLAYING_NEXT:
        ESP_LOGI(TAG, "NEXT");
        break;
    case AUDIO_PLAYER_CALLBACK_EVENT_PLAYING:
        ESP_LOGI(TAG, "PLAYING");
        break;
    case AUDIO_PLAYER_CALLBACK_EVENT_PAUSE:
        ESP_LOGI(TAG, "PAUSE");
        break;
    case AUDIO_PLAYER_CALLBACK_EVENT_SHUTDOWN:
        ESP_LOGI(TAG, "SHUTDOWN");
        break;
    case AUDIO_PLAYER_CALLBACK_EVENT_UNKNOWN_FILE_TYPE:
        ESP_LOGI(TAG, "UNKNOWN FILE");
        break;
    case AUDIO_PLAYER_CALLBACK_EVENT_UNKNOWN:
        ESP_LOGI(TAG, "UNKNOWN");
        break;
    }
}

static esp_err_t bsp_audio_reconfig_clk(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch)
{
    esp_err_t ret = ESP_OK;

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = rate,
        .channel = ch,
        .bits_per_sample = bits_cfg,
    };

    ret = esp_codec_dev_close(play_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to close codec dev");
        return ret;
    }

    ret = esp_codec_dev_open(play_dev_handle, &fs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open codec dev with new config");
    }
    return ret;
}

static esp_err_t bsp_audio_write(void* audio_buffer, size_t len, size_t* bytes_written, uint32_t timeout_ms)
{
    esp_err_t ret = esp_codec_dev_write(play_dev_handle, audio_buffer, len);
    *bytes_written = (ret == ESP_OK) ? len : 0;
    return ret;
}

static void bsp_codec_init()
{
    play_dev_handle = bsp_audio_codec_speaker_init();
    assert((play_dev_handle) && "play_dev_handle not initialized");
}

esp_err_t audio_play_start()
{
    esp_err_t ret = ESP_OK;

    bsp_codec_init();

    audio_player_config_t config = {
        .mute_fn = app_mute_function,
        .write_fn = app_audio_write,
        .clk_set_fn = bsp_audio_reconfig_clk,
        .priority = 5
    };
    ESP_ERROR_CHECK(audio_player_new(config));
    audio_player_callback_register(audio_callback, NULL);
    return ret;
}
