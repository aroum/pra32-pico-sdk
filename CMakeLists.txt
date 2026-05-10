cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

# Board Selection logic
if(NOT DEFINED CURRENT_BOARD)
    file(STRINGS "include/config.h" BOARD_LINE REGEX "^#define CURRENT_BOARD BOARD_")
    if(BOARD_LINE MATCHES "BOARD_NIZKOTENO")
        set(CURRENT_BOARD 2)
    elseif(BOARD_LINE MATCHES "BOARD_OMSK")
        set(CURRENT_BOARD 3)
    else()
        set(CURRENT_BOARD 1)
    endif()
endif()

if(CURRENT_BOARD EQUAL 2)
    set(PROJECT_NAME_STR "nizkoteno")
elseif(CURRENT_BOARD EQUAL 3)
    set(PROJECT_NAME_STR "omsk")
else()
    set(PROJECT_NAME_STR "pra32")
endif()

# set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(${PROJECT_NAME_STR} C CXX ASM)

pico_sdk_init()

add_executable(${PROJECT_NAME_STR}
    src/main.cpp
    src/usb_descriptors.c
    src/i2s_audio.c
    src/ui_state.cpp
    src/button_driver.cpp
    src/encoder_driver.cpp
    src/ui_oled.cpp
    src/led_driver.cpp
    src/WS2812.cpp
    src/sequencer.cpp
)

# u8g2 library (submodule or external)
set(U8G2_DIR ${CMAKE_CURRENT_LIST_DIR}/include/u8g2)
if (NOT EXISTS ${U8G2_DIR}/csrc)
    set(U8G2_DIR ${CMAKE_CURRENT_LIST_DIR}/../sw/lib/u8g2)
endif()
if (EXISTS ${U8G2_DIR}/csrc)
    target_include_directories(${PROJECT_NAME_STR} PRIVATE ${U8G2_DIR}/csrc)
    file(GLOB U8G2_SRC ${U8G2_DIR}/csrc/*.c)
    target_sources(${PROJECT_NAME_STR} PRIVATE ${U8G2_SRC})
endif()

pico_generate_pio_header(${PROJECT_NAME_STR} ${CMAKE_CURRENT_LIST_DIR}/src/audio_i2s.pio)
pico_generate_pio_header(${PROJECT_NAME_STR} ${CMAKE_CURRENT_LIST_DIR}/src/audio_sdm.pio)
pico_generate_pio_header(${PROJECT_NAME_STR} ${CMAKE_CURRENT_LIST_DIR}/src/debounce.pio)
pico_generate_pio_header(${PROJECT_NAME_STR} ${CMAKE_CURRENT_LIST_DIR}/src/ws2812.pio)

# Pull in common dependencies
target_link_libraries(${PROJECT_NAME_STR}
    pico_stdlib
    hardware_pio
    hardware_dma
    hardware_i2c
    hardware_adc
    hardware_flash
    hardware_sync
    hardware_pwm
    hardware_clocks
    pico_multicore
    tinyusb_device
    tinyusb_board
)

target_include_directories(${PROJECT_NAME_STR} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/../sw/src
)

# Enable USB MIDI
target_compile_definitions(${PROJECT_NAME_STR} PRIVATE
    CFG_TUSB_MCU=OPT_MCU_RP2040
    CFG_TUD_MIDI=1
    CFG_TUD_CDC=1
    CURRENT_BOARD=${CURRENT_BOARD}
)

pico_enable_stdio_usb(${PROJECT_NAME_STR} 1)
pico_enable_stdio_uart(${PROJECT_NAME_STR} 0)

pico_add_extra_outputs(${PROJECT_NAME_STR})
