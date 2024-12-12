# Dynamically include all .c files from relevant directories
file(GLOB_RECURSE UI_SOURCES "ui/*.c")
file(GLOB_RECURSE IR_SOURCES "ir_nec/*.c")
file(GLOB_RECURSE FONT_SOURCES "ui/fonts/*.c")
file(GLOB_RECURSE IMAGE_SOURCES "ui/imgs/*.c")

idf_component_register(
    SRCS
        "app_main.c"
        "voice_announcement.c"
        "settings.c"
        "app_audio.c"
        ${UI_SOURCES}
        ${IR_SOURCES}
        ${FONT_SOURCES}
        ${IMAGE_SOURCES}
    INCLUDE_DIRS
        "."
        "ui"
        "ui/fonts"
        "ui/imgs"
        "ui/layer_manage"
        "ir_nec"
    PRIV_REQUIRES
        nvs_flash  # Added earlier for nvs_flash.h
        esp_wifi   # Add esp_wifi for esp_wifi.h
)

spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-cast-function-type)