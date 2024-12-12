/* SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#include "voice_announcement.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "app_audio.h"

static const char* TAG = "voice_announcement";

#define EVENT_BIT_BRIGHTNESS_CHANGED (1 << 0)
#define TASK_STACK_SIZE              4096
#define TASK_PRIORITY                5

static int current_brightness = 0;

// Function to update brightness level (to be called in the lighting control code)
void update_brightness(int brightness) {
    if (brightness != current_brightness) {
        current_brightness = brightness;
        ESP_LOGI(TAG, "Brightness updated to %d", brightness);
        xEventGroupSetBits(event_group, EVENT_BIT_BRIGHTNESS_CHANGED);
    }
}

// Task for voice announcements
void voice_announcement_task(void* arg) {
    static TickType_t last_announcement_time = 0;

    while (1) {
        // Wait for the brightness change event
        xEventGroupWaitBits(event_group, EVENT_BIT_BRIGHTNESS_CHANGED, pdTRUE, pdFALSE, portMAX_DELAY);

        TickType_t now = xTaskGetTickCount();
        if ((now - last_announcement_time) < pdMS_TO_TICKS(1000)) {
            ESP_LOGI(TAG, "Skipping rapid announcements.");
            continue; // Debounce to avoid rapid announcements
        }
        last_announcement_time = now;

        // Announce the current brightness level
        ESP_LOGI(TAG, "Announcing brightness level: %d", current_brightness);

        char filepath[50];
        sprintf(filepath, "/spiffs/brightness_%d.mp3", current_brightness);

        // Attempt to play the audio file directly
        esp_err_t ret = play_audio_file(filepath);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Brightness level %d announced successfully", current_brightness);
        }
        else {
            ESP_LOGE(TAG, "Failed to play audio file: %s, error code: %d", filepath, ret);
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // Short delay to avoid looping immediately
    }
}

// Initialization function
void voice_announcement_init(void) {
    event_group = xEventGroupCreate();
    if (event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return;
    }

    // Create the voice announcement task
    xTaskCreate(voice_announcement_task, "voice_announcement_task", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL);
}
