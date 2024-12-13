#ifndef UI_LIGHT_2COLOR_H
#define UI_LIGHT_2COLOR_H

#include "lvgl.h"
#include "lv_example_pub.h"

#ifdef __cplusplus
extern "C" {
#endif

    // Declare the layer structure
    extern lv_layer_t light_2color_Layer;

    // Declare the callback functions
    bool light_2color_layer_enter_cb(void* layer);
    void light_2color_layer_timer_cb(lv_timer_t* tmr);

#ifdef __cplusplus
}
#endif

#endif // UI_LIGHT_2COLOR_H
