#include "lvgl.h"
#include <stdio.h>
#include "lv_example_pub.h"
#include "lv_example_image.h"
#include "bsp/esp-bsp.h"
#include "freertos/event_groups.h"
#include "app_audio.h"

static bool light_2color_layer_enter_cb(void* layer);
static bool light_2color_layer_exit_cb(void* layer); // Declare properly
static void light_2color_layer_timer_cb(lv_timer_t* tmr);
static void ui_light_2color_init(lv_obj_t* parent);  // Forward declaration of init function

// Event group for brightness control
static EventGroupHandle_t light_event_group;
#define EVENT_BRIGHTNESS_0    (1 << 0)
#define EVENT_BRIGHTNESS_25   (1 << 1)
#define EVENT_BRIGHTNESS_50   (1 << 2)
#define EVENT_BRIGHTNESS_75   (1 << 3)
#define EVENT_BRIGHTNESS_100  (1 << 4)

lv_layer_t light_2color_Layer = {
    .lv_obj_name = "light_2color_Layer",
    .lv_obj_parent = NULL,
    .lv_obj_layer = NULL,
    .lv_show_layer = NULL,
    .enter_cb = light_2color_layer_enter_cb,
    .exit_cb = light_2color_layer_exit_cb,
    .timer_cb = light_2color_layer_timer_cb,
};

// Dummy exit callback to suppress warnings
static bool light_2color_layer_exit_cb(void* layer) {
    LV_LOG_USER("Exiting light_2color layer");
    return true;
}

static void ui_light_2color_init(lv_obj_t* parent) {
    lv_obj_t* page = lv_obj_create(parent);
    lv_obj_set_size(page, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_style_radius(page, 0, 0);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(page);

    lv_obj_t* img_light_bg = lv_img_create(page);
    lv_img_set_src(img_light_bg, &light_warm_bg);
    lv_obj_align(img_light_bg, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* label_pwm_set = lv_label_create(page);
    lv_obj_set_style_text_font(label_pwm_set, &HelveticaNeue_Regular_24, 0);
    lv_label_set_text(label_pwm_set, "50%");
    lv_obj_align(label_pwm_set, LV_ALIGN_CENTER, 0, 65);
}

static bool light_2color_layer_enter_cb(void* layer) {
    bool ret = false;
    LV_LOG_USER("Entering light_2color layer");

    lv_layer_t* create_layer = layer;
    if (create_layer->lv_obj_layer == NULL) {
        ret = true;
        create_layer->lv_obj_layer = lv_obj_create(lv_scr_act());
        lv_obj_remove_style_all(create_layer->lv_obj_layer);
        lv_obj_set_size(create_layer->lv_obj_layer, LV_HOR_RES, LV_VER_RES);

        ui_light_2color_init(create_layer->lv_obj_layer);

        light_event_group = xEventGroupCreate();
        if (!light_event_group) {
            ESP_LOGE("light_2color", "Failed to create event group");
            return false;
        }
    }
    return ret;
}

static void light_2color_layer_timer_cb(lv_timer_t* tmr) {
    // Simplified timer callback
    ESP_LOGI("light_2color", "Timer callback triggered");
}
