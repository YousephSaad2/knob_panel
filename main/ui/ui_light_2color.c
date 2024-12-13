#include "lvgl.h"
#include <stdio.h>
#include "lv_example_pub.h"
#include "lv_example_image.h"
#include "bsp/esp-bsp.h"
#include "ui_light_2color.h"
#include "freertos/event_groups.h"  // For event group
#include "app_audio.h"             // For audio playback

// Event group and brightness events
static EventGroupHandle_t light_event_group;
#define EVENT_BRIGHTNESS_0    (1 << 0)
#define EVENT_BRIGHTNESS_25   (1 << 1)
#define EVENT_BRIGHTNESS_50   (1 << 2)
#define EVENT_BRIGHTNESS_75   (1 << 3)
#define EVENT_BRIGHTNESS_100  (1 << 4)

// Define light_2color_Layer here
lv_layer_t light_2color_Layer = {
    .lv_obj_name = "light_2color_Layer",
    .lv_obj_parent = NULL,
    .lv_obj_layer = NULL,
    .lv_show_layer = NULL,
    .enter_cb = light_2color_layer_enter_cb,  // Ensure this matches the definition
    .exit_cb = NULL,                         // Add exit callback if necessary
    .timer_cb = light_2color_layer_timer_cb  // Ensure this matches the definition
};

typedef enum {
    LIGHT_CCK_WARM,
    LIGHT_CCK_COOL,
    LIGHT_CCK_MAX,
} LIGHT_CCK_TYPE;

typedef struct {
    uint8_t light_pwm;
    LIGHT_CCK_TYPE light_cck;
} light_set_attribute_t;

static light_set_attribute_t light_set_conf;

// Timer callback for voice announcement
static void voice_announcement_task(void* param) {
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(
            light_event_group,
            EVENT_BRIGHTNESS_0 | EVENT_BRIGHTNESS_25 | EVENT_BRIGHTNESS_50 |
            EVENT_BRIGHTNESS_75 | EVENT_BRIGHTNESS_100,
            pdTRUE,         // Clear bits after reading
            pdFALSE,        // Wait for any bit
            portMAX_DELAY   // Block indefinitely
        );

        // Handle brightness announcements
        if (bits & EVENT_BRIGHTNESS_0) {
            audio_handle_info(SOUND_TYPE_BRIGHTNESS_0);
        }
        else if (bits & EVENT_BRIGHTNESS_25) {
            audio_handle_info(SOUND_TYPE_BRIGHTNESS_25);
        }
        else if (bits & EVENT_BRIGHTNESS_50) {
            audio_handle_info(SOUND_TYPE_BRIGHTNESS_50);
        }
        else if (bits & EVENT_BRIGHTNESS_75) {
            audio_handle_info(SOUND_TYPE_BRIGHTNESS_75);
        }
        else if (bits & EVENT_BRIGHTNESS_100) {
            audio_handle_info(SOUND_TYPE_BRIGHTNESS_100);
        }
    }
}

// Initialization logic
static bool initialize_light_event_group() {
    light_event_group = xEventGroupCreate();
    if (!light_event_group) {
        ESP_LOGE("light_2color", "Failed to create event group");
        return false;
    }

    // Start the voice announcement task
    if (xTaskCreate(voice_announcement_task, "VoiceAnnouncementTask", 2048, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE("light_2color", "Failed to create voice announcement task");
        return false;
    }

    return true;
}

// Enter callback for the light layer
bool light_2color_layer_enter_cb(void* layer) {
    bool ret = false;

    LV_LOG_USER("Entering light 2-color layer");
    lv_layer_t* create_layer = layer;

    if (NULL == create_layer->lv_obj_layer) {
        ret = true;
        create_layer->lv_obj_layer = lv_obj_create(lv_scr_act());
        lv_obj_remove_style_all(create_layer->lv_obj_layer);
        lv_obj_set_size(create_layer->lv_obj_layer, LV_HOR_RES, LV_VER_RES);

        // Initialize light event group and tasks
        if (!initialize_light_event_group()) {
            ESP_LOGE("light_2color", "Failed to initialize light event group");
            return false;
        }
    }

    return ret;
}

// Timer callback for the light layer
void light_2color_layer_timer_cb(lv_timer_t* tmr) {
    static uint8_t prev_pwm = 0xFF;  // To track PWM changes
    static LIGHT_CCK_TYPE prev_cck = LIGHT_CCK_MAX;  // To track CCK changes

    // Avoid unnecessary updates
    if (light_set_conf.light_pwm == prev_pwm && light_set_conf.light_cck == prev_cck) {
        return; // No changes; skip processing
    }

    prev_pwm = light_set_conf.light_pwm;
    prev_cck = light_set_conf.light_cck;

    ESP_LOGI("light_2color", "Updating light - PWM: %d, CCK: %d", light_set_conf.light_pwm, light_set_conf.light_cck);

    // Set event bits based on brightness level
    switch (light_set_conf.light_pwm) {
    case 0:
        xEventGroupSetBits(light_event_group, EVENT_BRIGHTNESS_0);
        break;
    case 25:
        xEventGroupSetBits(light_event_group, EVENT_BRIGHTNESS_25);
        break;
    case 50:
        xEventGroupSetBits(light_event_group, EVENT_BRIGHTNESS_50);
        break;
    case 75:
        xEventGroupSetBits(light_event_group, EVENT_BRIGHTNESS_75);
        break;
    case 100:
        xEventGroupSetBits(light_event_group, EVENT_BRIGHTNESS_100);
        break;
    default:
        ESP_LOGW("light_2color", "Unknown brightness level: %d", light_set_conf.light_pwm);
        break;
    }

    // Example: Update RGB color based on settings (customize as needed)
    uint32_t RGB_color = (light_set_conf.light_cck == LIGHT_CCK_COOL) ?
        (0xFF * light_set_conf.light_pwm / 100) << 16 | (0xFF * light_set_conf.light_pwm / 100) << 8 | (0xFF * light_set_conf.light_pwm / 100) :
        (0xFF * light_set_conf.light_pwm / 100) << 16 | (0xFF * light_set_conf.light_pwm / 100) << 8 | (0x33 * light_set_conf.light_pwm / 100);

    bsp_led_rgb_set((RGB_color >> 16) & 0xFF, (RGB_color >> 8) & 0xFF, (RGB_color >> 0) & 0xFF);

    ESP_LOGI("light_2color", "RGB updated: 0x%08X", RGB_color);
}

int main(void) {
    // Initialize event group
    if (!initialize_light_event_group()) {
        ESP_LOGE("main", "Failed to initialize event group");
        return -1;
    }

    // Add main application logic here
    ESP_LOGI("main", "Application started");

    // Prevent warnings by actually using `light_set_conf`
    light_set_conf.light_pwm = 50;
    light_set_conf.light_cck = LIGHT_CCK_WARM;

    return 0;
}
