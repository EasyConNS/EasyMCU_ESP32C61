file(GLOB_RECURSE srcs "main.c" "src/*.c")
file(GLOB_RECURSE buffer_srcs "src/buffer/*.c")
file(GLOB_RECURSE transport_srcs "src/transport/*.c")


idf_component_register(SRCS "${srcs}" "${buffer_srcs}" "${transport_srcs}"
                    PRIV_REQUIRES bt nvs_flash mbedtls esp_driver_uart esp_driver_usb_serial_jtag
                    INCLUDE_DIRS "." "./include" "./include/buffer" "./include/transport")
