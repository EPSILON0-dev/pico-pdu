# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(pico_pdu C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

include(FreeRTOS_Kernel_import.cmake)

add_executable(pico_pdu 
    ${CMAKE_CURRENT_LIST_DIR}/src/pico_pdu.c 
    ${CMAKE_CURRENT_LIST_DIR}/src/pio_uart.c 
    ${CMAKE_CURRENT_LIST_DIR}/src/console.c 
    ${CMAKE_CURRENT_LIST_DIR}/src/usb.c 
    ${CMAKE_CURRENT_LIST_DIR}/src/relay.c 
    ${CMAKE_CURRENT_LIST_DIR}/src/monitor.c 
    ${CMAKE_CURRENT_LIST_DIR}/src/usb_descriptors.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/embedded_cli.c
)

pico_set_program_name(pico_pdu "pico_pdu")
pico_set_program_version(pico_pdu "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_usb(pico_pdu 1)

# Add the pio program to the build
pico_generate_pio_header(pico_pdu
    ${CMAKE_CURRENT_LIST_DIR}/src/uart_tx.pio
    ${CMAKE_CURRENT_LIST_DIR}/src/uart_rx.pio
)

# Add the standard library to the build
target_compile_definitions(pico_pdu PUBLIC
    PICO_ENABLE_USB_RESET_VIA_VENDOR_INTERFACE=1
    PICO_USB_RESET_SUPPORT_MS_OS_20_DESCRIPTOR=1
    PICO_USB_RESET_MS_OS_20_DESCRIPTOR_ITF=4
    CFG_TUSB_OS=OPT_OS_PICO
)

# Add the standard library to the build
target_link_libraries(pico_pdu
    pico_async_context_freertos
    FreeRTOS-Kernel-Heap4
    pico_stdlib
    pico_usb_reset
    hardware_pio
    hardware_dma
    pico_unique_id
    tinyusb_device 
    tinyusb_board
)

# Add the standard include files to the build
target_include_directories(pico_pdu PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/src
)

# Add extra uf2 outputs
pico_add_extra_outputs(pico_pdu)

