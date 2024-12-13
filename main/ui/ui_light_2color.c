/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "lvgl.h"
#include <stdio.h>

#include "lv_example_pub.h"
#include "lv_example_image.h"
#include "bsp/esp-bsp.h"
#include "app_audio.h" // Added for handling audio functionalities
#include "settings.h"  // Added for handling application settings
#include "freertos/FreeRTOS.h" // Added for FreeRTOS task management
#include "freertos/queue.h"    // Added for inter-task communication
#include "freertos/event_groups.h" // Added for event synchronization
#include "freertos/task.h"         // Added for task creation and management

 // Callback function declarations for layer management
static bool light_2color_layer_enter_cb(void* layer);
static bool light_2color_layer_exit_cb(void* layer);
static void light_2color_layer_timer_cb(lv_timer_t* tmr);
static EventGroupHandle_t light_event_group; // Event group for brightness control synchronization

// Event bits for brightness levels
#define EVENT_BRIGHTNESS_0    (1 << 0)
#define EVENT_BRIGHTNESS_25   (1 << 1)
#define EVENT_BRIGHTNESS_50   (1 << 2)
#define EVENT_BRIGHTNESS_75   (1 << 3)
#define EVENT_BRIGHTNESS_100  (1 << 4)

// Enum for light color temperature types
typedef enum {
    LIGHT_CCK_WARM,
    LIGHT_CCK_COOL,
    LIGHT_CCK_MAX, // Represents the number of color temperature modes
} LIGHT_CCK_TYPE;

// Struct to store light attributes like brightness and color temperature
typedef struct {
    uint8_t light_pwm;  // PWM value for brightness (0-100)
    LIGHT_CCK_TYPE light_cck; // Color temperature type
} light_set_attribute_t;

// Struct to hold UI image assets for different states
typedef struct {
    const lv_img_dsc_t* img_bg[2]; // Background images for warm and cool modes
    const lv_img_dsc_t* img_pwm_25[2]; // Images for 25% brightness
    const lv_img_dsc_t* img_pwm_50[2]; // Images for 50% brightness
    const lv_img_dsc_t* img_pwm_75[2]; // Images for 75% brightness
    const lv_img_dsc_t* img_pwm_100[2]; // Images for 100% brightness
} ui_light_img_t;

static lv_obj_t* page; // Main page object
static time_out_count time_20ms, time_500ms; // Timeout objects for timing

// UI elements for light control
static lv_obj_t* img_light_bg, * label_pwm_set;
static lv_obj_t* img_light_pwm_25, * img_light_pwm_50, * img_light_pwm_75, * img_light_pwm_100, * img_light_pwm_0;

static light_set_attribute_t light_set_conf, light_xor; // Current and XORed light settings for state tracking

// Predefined light images for different brightness and color temperatures
static const ui_light_img_t light_image = {
    {&light_warm_bg,     &light_cool_bg},
    {&light_warm_25,     &light_cool_25},
    {&light_warm_50,     &light_cool_50},
    {&light_warm_75,     &light_cool_75},
    {&light_warm_100,    &light_cool_100},
};

// Layer definition for light control
lv_layer_t light_2color_Layer = {
    .lv_obj_name = "light_2color_Layer",
    .lv_obj_parent = NULL,
    .lv_obj_layer = NULL,
    .lv_show_layer = NULL,
    .enter_cb = light_2color_layer_enter_cb,
    .exit_cb = light_2color_layer_exit_cb,
    .timer_cb = light_2color_layer_timer_cb,
};

// Callback function for handling events on the light control UI
static void light_2color_event_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (LV_EVENT_FOCUSED == code) {
        // Enable editing mode when focused
        lv_group_set_editing(lv_group_get_default(), true);
    }
    else if (LV_EVENT_KEY == code) {
        uint32_t key = lv_event_get_key(e);
        if (is_time_out(&time_500ms)) { // Adjust brightness only after timeout
            if (LV_KEY_RIGHT == key) {
                if (light_set_conf.light_pwm < 100) {
                    light_set_conf.light_pwm += 25; // Increment brightness by 25%
                }
            }
            else if (LV_KEY_LEFT == key) {
                if (light_set_conf.light_pwm > 0) {
                    light_set_conf.light_pwm -= 25; // Decrement brightness by 25%
                }
            }
        }
    }
    else if (LV_EVENT_CLICKED == code) {
        // Toggle color temperature on click
        light_set_conf.light_cck =
            (LIGHT_CCK_WARM == light_set_conf.light_cck) ? (LIGHT_CCK_COOL) : (LIGHT_CCK_WARM);
    }
    else if (LV_EVENT_LONG_PRESSED == code) {
        // Navigate to menu layer on long press
        lv_indev_wait_release(lv_indev_get_next(NULL));
        ui_remove_all_objs_from_encoder_group();
        lv_func_goto_layer(&menu_layer);
    }
}

// Function to initialize the UI for the two-color light control
void ui_light_2color_init(lv_obj_t* parent)
{
    // Initialize XOR settings with default values to ensure state tracking
    light_xor.light_pwm = 0xFF; // Set to an invalid value to trigger the first update
    light_xor.light_cck = LIGHT_CCK_MAX; // Invalid state to trigger update

    // Set default light settings (50% brightness and warm color temperature)
    light_set_conf.light_pwm = 50;
    light_set_conf.light_cck = LIGHT_CCK_WARM;

    // Create the main page object
    page = lv_obj_create(parent);
    lv_obj_set_size(page, LV_HOR_RES, LV_VER_RES); // Set size to match screen resolution

    // Remove borders and rounded corners for a clean UI
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_style_radius(page, 0, 0);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE); // Disable scrolling
    lv_obj_center(page);

    // Create and configure the background image
    img_light_bg = lv_img_create(page);
    lv_img_set_src(img_light_bg, &light_warm_bg); // Set default background (warm mode)
    lv_obj_align(img_light_bg, LV_ALIGN_CENTER, 0, 0);

    // Create and configure the brightness percentage label
    label_pwm_set = lv_label_create(page);
    lv_obj_set_style_text_font(label_pwm_set, &HelveticaNeue_Regular_24, 0);
    if (light_set_conf.light_pwm) {
        lv_label_set_text_fmt(label_pwm_set, "%d%%", light_set_conf.light_pwm); // Display brightness percentage
    }
    else {
        lv_label_set_text(label_pwm_set, "--"); // Display dashes if brightness is 0
    }
    lv_obj_align(label_pwm_set, LV_ALIGN_CENTER, 0, 65);

    // Create and configure images for each brightness level
    img_light_pwm_0 = lv_img_create(page);
    lv_img_set_src(img_light_pwm_0, &light_close_status);
    lv_obj_add_flag(img_light_pwm_0, LV_OBJ_FLAG_HIDDEN); // Start as hidden
    lv_obj_align(img_light_pwm_0, LV_ALIGN_TOP_MID, 0, 0);

    img_light_pwm_25 = lv_img_create(page);
    lv_img_set_src(img_light_pwm_25, &light_warm_25);
    lv_obj_align(img_light_pwm_25, LV_ALIGN_TOP_MID, 0, 0);

    img_light_pwm_50 = lv_img_create(page);
    lv_img_set_src(img_light_pwm_50, &light_warm_50);
    lv_obj_align(img_light_pwm_50, LV_ALIGN_TOP_MID, 0, 0);

    img_light_pwm_75 = lv_img_create(page);
    lv_img_set_src(img_light_pwm_75, &light_warm_75);
    lv_obj_add_flag(img_light_pwm_75, LV_OBJ_FLAG_HIDDEN); // Hidden by default
    lv_obj_align(img_light_pwm_75, LV_ALIGN_TOP_MID, 0, 0);

    img_light_pwm_100 = lv_img_create(page);
    lv_img_set_src(img_light_pwm_100, &light_warm_100);
    lv_obj_add_flag(img_light_pwm_100, LV_OBJ_FLAG_HIDDEN); // Hidden by default
    lv_obj_align(img_light_pwm_100, LV_ALIGN_TOP_MID, 0, 0);

    // Add event callbacks for key user interactions
    lv_obj_add_event_cb(page, light_2color_event_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(page, light_2color_event_cb, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(page, light_2color_event_cb, LV_EVENT_LONG_PRESSED, NULL);
    lv_obj_add_event_cb(page, light_2color_event_cb, LV_EVENT_CLICKED, NULL);
    ui_add_obj_to_encoder_group(page); // Add page to encoder group for navigation
}

// Task to handle voice announcements for brightness levels
static void voice_announcement_task(void* param) {
    while (1) {
        // Wait for any brightness event bit to be set
        EventBits_t bits = xEventGroupWaitBits(
            light_event_group,
            EVENT_BRIGHTNESS_0 | EVENT_BRIGHTNESS_25 |
            EVENT_BRIGHTNESS_50 | EVENT_BRIGHTNESS_75 |
            EVENT_BRIGHTNESS_100,
            pdTRUE,         // Clear the bits after reading
            pdFALSE,        // Wait for any one bit
            portMAX_DELAY   // Block indefinitely
        );

        // Play the corresponding audio for the triggered brightness event
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

// Callback for entering the two-color light control layer
static bool light_2color_layer_enter_cb(void* layer)
{
    bool ret = false;

    LV_LOG_USER(""); // Log layer entry for debugging
    lv_layer_t* create_layer = layer;
    if (NULL == create_layer->lv_obj_layer) {
        ret = true;

        // Create the layer's main object
        create_layer->lv_obj_layer = lv_obj_create(lv_scr_act());
        lv_obj_remove_style_all(create_layer->lv_obj_layer);
        lv_obj_set_size(create_layer->lv_obj_layer, LV_HOR_RES, LV_VER_RES);

        // Initialize UI elements
        ui_light_2color_init(create_layer->lv_obj_layer);
        set_time_out(&time_20ms, 20); // Set 20ms timeout for periodic updates
        set_time_out(&time_500ms, 200); // Set 500ms timeout for interactions
        light_event_group = xEventGroupCreate(); // Create event group for brightness

        if (light_event_group == NULL) {
            printf("Error: Failed to create event group.\n");
            return false; // Abort if event group creation fails
        }

        // Create the task for voice announcements
        xTaskCreate(
            voice_announcement_task,  // Task function
            "VoiceAnnouncementTask",  // Task name
            2048,                     // Stack size
            NULL,                     // Parameters for the task
            5,                        // Task priority
            NULL                      // Handle (not used)
        );
    }

    return ret;
}

// Callback for exiting the layer
static bool light_2color_layer_exit_cb(void* layer)
{
    LV_LOG_USER(""); // Log layer exit for debugging
    bsp_led_rgb_set(0x00, 0x00, 0x00); // Turn off LED lights
    return true;
}

// Timer callback for managing updates in the two-color light control layer
static void light_2color_layer_timer_cb(lv_timer_t* tmr)
{
    uint32_t RGB_color = 0xFF; // Default RGB color value

    feed_clock_time(); // Update system clock

    if (is_time_out(&time_20ms)) { // Check if the 20ms timeout has elapsed

        // Check for changes in brightness or color temperature settings
        if ((light_set_conf.light_pwm ^ light_xor.light_pwm) ||
            (light_set_conf.light_cck ^ light_xor.light_cck)) {

            // Update XOR settings to reflect current state
            light_xor.light_pwm = light_set_conf.light_pwm;
            light_xor.light_cck = light_set_conf.light_cck;

            // Set event bits based on the current brightness level
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
            default:
                break;
            }

            // Calculate and set the RGB color based on brightness and mode
            if (LIGHT_CCK_COOL == light_xor.light_cck) {
                RGB_color = (0xFF * light_xor.light_pwm / 100) << 16 |
                    (0xFF * light_xor.light_pwm / 100) << 8 |
                    (0xFF * light_xor.light_pwm / 100);
            }
            else {
                RGB_color = (0xFF * light_xor.light_pwm / 100) << 16 |
                    (0xFF * light_xor.light_pwm / 100) << 8 |
                    (0x33 * light_xor.light_pwm / 100);
            }
            bsp_led_rgb_set((RGB_color >> 16) & 0xFF,
                (RGB_color >> 8) & 0xFF,
                (RGB_color >> 0) & 0xFF);

            // Update UI images and label based on brightness level
            lv_obj_add_flag(img_light_pwm_100, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(img_light_pwm_75, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(img_light_pwm_50, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(img_light_pwm_25, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(img_light_pwm_0, LV_OBJ_FLAG_HIDDEN);

            if (light_set_conf.light_pwm) {
                lv_label_set_text_fmt(label_pwm_set, "%d%%", light_set_conf.light_pwm);
            }
            else {
                lv_label_set_text(label_pwm_set, "--");
            }

            uint8_t cck_set = (uint8_t)light_xor.light_cck;

            // Update visible image based on brightness level
            switch (light_xor.light_pwm) {
            case 100:
                lv_obj_clear_flag(img_light_pwm_100, LV_OBJ_FLAG_HIDDEN);
                lv_img_set_src(img_light_pwm_100, light_image.img_pwm_100[cck_set]);
                break;
            case 75:
                lv_obj_clear_flag(img_light_pwm_75, LV_OBJ_FLAG_HIDDEN);
                lv_img_set_src(img_light_pwm_75, light_image.img_pwm_75[cck_set]);
                break;
            case 50:
                lv_obj_clear_flag(img_light_pwm_50, LV_OBJ_FLAG_HIDDEN);
                lv_img_set_src(img_light_pwm_50, light_image.img_pwm_50[cck_set]);
                break;
            case 25:
                lv_obj_clear_flag(img_light_pwm_25, LV_OBJ_FLAG_HIDDEN);
                lv_img_set_src(img_light_pwm_25, light_image.img_pwm_25[cck_set]);
                lv_img_set_src(img_light_bg, light_image.img_bg[cck_set]);
                break;
            case 0:
                lv_obj_clear_flag(img_light_pwm_0, LV_OBJ_FLAG_HIDDEN);
                lv_img_set_src(img_light_bg, &light_close_bg);
                break;
            default:
                break;
            }
        }
    }
}
