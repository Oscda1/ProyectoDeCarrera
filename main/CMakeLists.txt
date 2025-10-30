idf_component_register(SRCS "sock.c" "espnow.c" "main.c" "gpios.c" "first_time_config.c"
                    INCLUDE_DIRS ".")
spiffs_create_partition_image(storage ../data FLASH_IN_PROJECT)
