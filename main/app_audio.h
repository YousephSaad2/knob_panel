#ifndef APP_AUDIO_H
#define APP_AUDIO_H

#include "esp_err.h"

typedef enum {
    SOUND_TYPE_KNOB,
    SOUND_TYPE_SNORE,
    SOUND_TYPE_WASH_END_CN,
    SOUND_TYPE_WASH_END_EN,
    SOUND_TYPE_FACTORY,
    SOUND_TYPE_BRIGHTNESS_0,
    SOUND_TYPE_BRIGHTNESS_25,
    SOUND_TYPE_BRIGHTNESS_50,
    SOUND_TYPE_BRIGHTNESS_75,
    SOUND_TYPE_BRIGHTNESS_100
} PDM_SOUND_TYPE;

esp_err_t audio_force_quite(bool ret);

esp_err_t audio_handle_info(PDM_SOUND_TYPE voice);

esp_err_t audio_play_start();

// Function to play an audio file from SPIFFS
esp_err_t play_audio_file(const char* filepath);

#endif // APP_AUDIO_H
