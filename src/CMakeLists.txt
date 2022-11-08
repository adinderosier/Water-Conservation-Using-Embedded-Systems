add_executable(main
        main.c
)

target_link_libraries(main 
        pico_stdlib 
        pico_cyw43_arch_none
        FreeRTOS
        )


pico_enable_stdio_usb(main 1)

pico_add_extra_outputs(main)
