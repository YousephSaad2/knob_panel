/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "app_audio.h"
#include "settings.h"
#include "lv_example_pub.h"
#include "bsp/esp-bsp.h"
#include "voice_announcement.h"

#include "esp_spiffs.h"

static const char *TAG = "main";

void list_files_in_spiffs(void) {
    ESP_LOGI(TAG, "Listing files in /spiffs:");

    DIR* dir = opendir("/spiffs");
    if (dir == NULL) {
        ESP_LOGE(TAG, "Failed to open /spiffs directory");
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        ESP_LOGI(TAG, "File: %s", entry->d_name);
    }

    closedir(dir);
}

void initialize_spiffs(void) {
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount or format SPIFFS (%s)", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "SPIFFS initialized successfully");
}

#define MEMORY_MONITOR 0

#if MEMORY_MONITOR

#define ARRAY_SIZE_OFFSET   5   //Increase this if print_real_time_stats returns ESP_ERR_INVALID_SIZE

/**
 * @brief   Function to print the CPU usage of tasks over a given duration.
 *
 * This function will measure and print the CPU usage of tasks over a specified
 * number of ticks (i.e. real time stats). This is implemented by simply calling
 * uxTaskGetSystemState() twice separated by a delay, then calculating the
 * differences of task run times before and after the delay.
 *
 * @note    If any tasks are added or removed during the delay, the stats of
 *          those tasks will not be printed.
 * @note    This function should be called from a high priority task to minimize
 *          inaccuracies with delays.
 * @note    When running in dual core mode, each core will correspond to 50% of
 *          the run time.
 *
 * @param   xTicksToWait    Period of stats measurement
 *
 * @return
 *  - ESP_OK                Success
 *  - ESP_ERR_NO_MEM        Insufficient memory to allocated internal arrays
 *  - ESP_ERR_INVALID_SIZE  Insufficient array size for uxTaskGetSystemState. Trying increasing ARRAY_SIZE_OFFSET
 *  - ESP_ERR_INVALID_STATE Delay duration too short
 */
static esp_err_t print_real_time_stats(TickType_t xTicksToWait)
{
    TaskStatus_t *start_array = NULL, *end_array = NULL;
    UBaseType_t start_array_size, end_array_size;
    uint32_t start_run_time, end_run_time;
    esp_err_t ret;

    //Allocate array to store current task states
    start_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
    start_array = malloc(sizeof(TaskStatus_t) * start_array_size);
    if (start_array == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }
    //Get current task states
    start_array_size = uxTaskGetSystemState(start_array, start_array_size, &start_run_time);
    if (start_array_size == 0) {
        ret = ESP_ERR_INVALID_SIZE;
        goto exit;
    }

    vTaskDelay(xTicksToWait);

    //Allocate array to store tasks states post delay
    end_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
    end_array = malloc(sizeof(TaskStatus_t) * end_array_size);
    if (end_array == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }
    //Get post delay task states
    end_array_size = uxTaskGetSystemState(end_array, end_array_size, &end_run_time);
    if (end_array_size == 0) {
        ret = ESP_ERR_INVALID_SIZE;
        goto exit;
    }

    //Calculate total_elapsed_time in units of run time stats clock period.
    uint32_t total_elapsed_time = (end_run_time - start_run_time);
    if (total_elapsed_time == 0) {
        ret = ESP_ERR_INVALID_STATE;
        goto exit;
    }

    printf("| Task \t\t| Run Time \t| Percentage\n");
    //Match each task in start_array to those in the end_array
    for (int i = 0; i < start_array_size; i++) {
        int k = -1;
        for (int j = 0; j < end_array_size; j++) {
            if (start_array[i].xHandle == end_array[j].xHandle) {
                k = j;
                //Mark that task have been matched by overwriting their handles
                start_array[i].xHandle = NULL;
                end_array[j].xHandle = NULL;
                break;
            }
        }
        //Check if matching task found
        if (k >= 0) {
            uint32_t task_elapsed_time = end_array[k].ulRunTimeCounter - start_array[i].ulRunTimeCounter;
            uint32_t percentage_time = (task_elapsed_time * 100UL) / (total_elapsed_time * portNUM_PROCESSORS);
            printf("| %s \t\t| %d \t| %d%%\n", start_array[i].pcTaskName, task_elapsed_time, percentage_time);
        }
    }

    //Print unmatched tasks
    for (int i = 0; i < start_array_size; i++) {
        if (start_array[i].xHandle != NULL) {
            printf("| %s | Deleted\n", start_array[i].pcTaskName);
        }
    }
    for (int i = 0; i < end_array_size; i++) {
        if (end_array[i].xHandle != NULL) {
            printf("| %s | Created\n", end_array[i].pcTaskName);
        }
    }
    ret = ESP_OK;

exit:    //Common return path
    free(start_array);
    free(end_array);
    return ret;
}

static void monitor_task(void *arg)
{
    (void) arg;
    const int STATS_TICKS = pdMS_TO_TICKS(2 * 1000);

    while (true) {
        ESP_LOGI(TAG, "System Info Trace");
        printf("\tDescription\tInternal\tSPIRAM\n");
        printf("Current Free Memory\t%d\t\t%d\n",
               heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL),
               heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        printf("Largest Free Block\t%d\t\t%d\n",
               heap_caps_get_largest_free_block(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL),
               heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
        printf("Min. Ever Free Size\t%d\t\t%d\n",
               heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL),
               heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));

        printf("Getting real time stats over %d ticks\n", STATS_TICKS);
        if (print_real_time_stats(STATS_TICKS) == ESP_OK) {
            printf("Real time stats obtained\n");
        } else {
            printf("Error getting real time stats\n");
        }

        vTaskDelay(STATS_TICKS);
    }

    vTaskDelete(NULL);
}

static void sys_monitor_start(void)
{
    BaseType_t ret_val = xTaskCreatePinnedToCore(monitor_task, "Monitor Task", 4 * 1024, NULL, configMAX_PRIORITIES - 3, NULL, 0);
    ESP_ERROR_CHECK_WITHOUT_ABORT((pdPASS == ret_val) ? ESP_OK : ESP_FAIL);
}
#endif


esp_err_t bsp_board_init(void)
{
    ESP_ERROR_CHECK(bsp_led_init());
    ESP_ERROR_CHECK(bsp_spiffs_mount());
    return ESP_OK;
}

EventGroupHandle_t event_group;

void app_main(void)
{
    ESP_LOGI(TAG, "Compile time: %s %s", __DATE__, __TIME__);

    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(settings_read_parameter_from_nvs());

    // Initialize SPIFFS
    initialize_spiffs();

    // List files in SPIFFS
    list_files_in_spiffs();

    // Initialize board peripherals and display
    bsp_display_start();

    // Log heap space before LVGL initialization
    ESP_LOGI("Heap Info", "Total Free Heap: %d bytes", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    ESP_LOGI("Heap Info", "Minimum Free Heap: %d bytes", heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));

    ESP_LOGI(TAG, "Display LVGL demo");
    ui_obj_to_encoder_init();
    lv_create_home(&boot_Layer);
    lv_create_clock(&clock_screen_layer, TIME_ENTER_CLOCK_2MIN);
    bsp_display_unlock();
    vTaskDelay(pdMS_TO_TICKS(500));
    bsp_display_backlight_on();

    // Initialize board components
    bsp_board_init();
    audio_play_start();

    // Initialize event group for voice announcements
    event_group = xEventGroupCreate();
    if (!event_group) {
        ESP_LOGE(TAG, "Failed to create event group");
        return;
    }

    // Start the voice announcement task
    BaseType_t task_created = xTaskCreate(
        voice_announcement_task,
        "Voice Announcement Task",
        TASK_STACK_SIZE,
        NULL,
        TASK_PRIORITY,
        NULL
    );
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create voice announcement task");
        return;
    }

    // Optional: Start memory monitoring (if enabled)
#if MEMORY_MONITOR
    sys_monitor_start();
#endif


    // Log stack usage of all tasks
    TaskStatus_t* tasks;
    UBaseType_t num_tasks;
    num_tasks = uxTaskGetNumberOfTasks();
    tasks = pvPortMalloc(num_tasks * sizeof(TaskStatus_t));

    if (tasks) {
        num_tasks = uxTaskGetSystemState(tasks, num_tasks, NULL);
        for (int i = 0; i < num_tasks; i++) {
            ESP_LOGI("Task Info", "Task: %s, Stack High Water Mark: %d",
                tasks[i].pcTaskName, tasks[i].usStackHighWaterMark);
        }
        vPortFree(tasks);
    }

    ESP_LOGI(TAG, "System initialized successfully!");
}
