# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

set(FREERTOS_KERNEL_PATH "C:/FreeRTOS-Kernel")
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)


project(PainelControle C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

include_directories(${CMAKE_SOURCE_DIR}/lib)

# Add executable. Default name is the project name, version 0.1

add_executable(PainelControle PainelControle.c lib/ssd1306.c lib/matriz.c)

pico_set_program_name(PainelControle "PainelControle")
pico_set_program_version(PainelControle "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(PainelControle 1)
pico_enable_stdio_usb(PainelControle 1)


# Add the standard include files to the build
target_include_directories(PainelControle PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(PainelControle 
        pico_stdlib
        hardware_pio
        hardware_gpio
        FreeRTOS-Kernel 
        FreeRTOS-Kernel-Heap4
        hardware_pwm
        hardware_clocks
        hardware_i2c
        )

#Gerar arquivo cabeçalho pio
pico_generate_pio_header(${CMAKE_PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/lib/pio_matrix.pio)

pico_add_extra_outputs(PainelControle)

