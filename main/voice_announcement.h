#ifndef VOICE_ANNOUNCEMENT_H
#define VOICE_ANNOUNCEMENT_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

extern EventGroupHandle_t event_group;

// Task stack size and priority
#define TASK_STACK_SIZE 4096  // Define the stack size here
#define TASK_PRIORITY 5
#define EVENT_BIT_BRIGHTNESS_CHANGED (1 << 0)

void update_brightness(int brightness);
void voice_announcement_task(void* arg);

#endif // VOICE_ANNOUNCEMENT_H
