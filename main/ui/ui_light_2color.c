#include "lvgl.h"
#include <stdio.h>
#include "lv_example_pub.h"
#include "lv_example_image.h"
#include "bsp/esp-bsp.h"
#include "freertos/event_groups.h"  // Added for event group
#include "app_audio.h"             // Added for audio playback

static bool light_2color_layer_enter_cb(void* layer);
static bool light_2color_layer_exit_cb(void* layer);
static void light_2color_layer_timer_cb(lv_timer_t* tmr);

// Define event group and brightness events
static EventGroupHandle_t light_event_group;
#define EVENT_BRIGHTNESS_0    (1 << 0)
#define EVENT_BRIGHTNESS_25   (1 << 1)
#define EVENT_BRIGHTNESS_50   (1 << 2)
#define EVENT_BRIGHTNESS_75   (1 << 3)
#define EVENT_BRIGHTNESS_100  (1 << 4)

typedef enum {
    LIGHT_CCK_WARM,
    LIGHT_CCK_COOL,
    LIGHT_CCK_MAX,
} LIGHT_CCK_TYPE;

typedef struct {
    uint8_t light_pwm;
    LIGHT_CCK_TYPE light_cck;
} light_set_attribute_t;

typedef struct {
    const lv_img_dsc_t* img_bg[2];
    const lv_img_dsc_t* img_pwm_25[2];
    const lv_img_dsc_t* img_pwm_50[2];
    const lv_img_dsc_t* img_pwm_75[2];
    const lv_img_dsc_t* img_pwm_100[2];
} ui_light_img_t;

static lv_obj_t* page;
static time_out_count time_20ms, time_500ms;

static lv_obj_t* img_light_bg, * label_pwm_set;
static lv_obj_t* img_light_pwm_25, * img_light_pwm_50, * img_light_pwm_75, * img_light_pwm_100, * img_light_pwm_0;

static light_set_attribute_t light_set_conf, light_xor;

static const ui_light_img_t light_image = {
    {&light_warm_bg, &light_cool_bg},
    {&light_warm_25, &light_cool_25},
    {&light_warm_50, &light_cool_50},
    {&light_warm_75, &light_cool_75},
    {&light_warm_100, &light_cool_100},
};

lv_layer_t light_2color_Layer = {
    .lv_obj_name = "light_2color_Layer",
    .lv_obj_parent = NULL,
    .lv_obj_layer = NULL,
    .lv_show_layer = NULL,
    .enter_cb = light_2color_layer_enter_cb,
    .exit_cb = light_2color_layer_exit_cb,
    .timer_cb = light_2color_layer_timer_cb,
};

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

static bool light_2color_layer_enter_cb(void* layer) {
    bool ret = false;

    LV_LOG_USER("");
    lv_layer_t* create_layer = layer;
    if (NULL == create_layer->lv_obj_layer) {
        ret = true;
        create_layer->lv_obj_layer = lv_obj_create(lv_scr_act());
        lv_obj_remove_style_all(create_layer->lv_obj_layer);
        lv_obj_set_size(create_layer->lv_obj_layer, LV_HOR_RES, LV_VER_RES);

        ui_light_2color_init(create_layer->lv_obj_layer);
        set_time_out(&time_20ms, 20);
        set_time_out(&time_500ms, 200);

        // Initialize the event group
        light_event_group = xEventGroupCreate();
        if (!light_event_group) {
            ESP_LOGE("light_2color", "Failed to create event group");
            return false;
        }

        // Start the voice announcement task
        xTaskCreate(voice_announcement_task, "VoiceAnnouncementTask", 2048, NULL, 5, NULL);
    }

    return ret;
}

static void light_2color_layer_timer_cb(lv_timer_t* tmr) {
    uint32_t RGB_color = 0xFF;

    feed_clock_time();

    if (is_time_out(&time_20ms)) {
        if ((light_set_conf.light_pwm ^ light_xor.light_pwm) || (light_set_conf.light_cck ^ light_xor.light_cck)) {
            light_xor.light_pwm = light_set_conf.light_pwm;
            light_xor.light_cck = light_set_conf.light_cck;

            // Set event bits based on brightness level
            switch (light_xor.light_pwm) {
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
            }
        }
    }
}
