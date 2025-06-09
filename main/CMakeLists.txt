idf_component_register(SRCS "gpios.c" "first_time_config.c" "espnow_example_main.c"
                    INCLUDE_DIRS ".")
spiffs_create_partition_image(storage ../data FLASH_IN_PROJECT)
