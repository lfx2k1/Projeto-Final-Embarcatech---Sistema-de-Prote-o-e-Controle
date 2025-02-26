# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
# == NÃO EDITAR AS LINHAS SEGUINTES para a extensão do VS Code do Raspberry Pi Pico funcionar ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PICO_BOARD pico CACHE STRING "Board type")

include(pico_sdk_import.cmake)
project(ADC_DisplayC C CXX ASM)
pico_sdk_init()

add_executable(ADC_DisplayC ADC_DisplayC.c lib/ssd1306.c)

pico_set_program_name(ADC_DisplayC "ADC_DisplayC")
pico_set_program_version(ADC_DisplayC "0.1")
pico_enable_stdio_uart(ADC_DisplayC 0)
pico_enable_stdio_usb(ADC_DisplayC 1)

# Gerar cabeçalhos PIO
file(MAKE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/generated)
pico_generate_pio_header(ADC_DisplayC ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)

# Link com as bibliotecas necessárias
target_link_libraries(ADC_DisplayC 
    pico_stdlib 
    hardware_i2c 
    hardware_adc 
    hardware_pwm 
    hardware_gpio 
    hardware_pio
    pico_bootsel_via_double_reset 
    pico_bootrom
)

# Adicione o diretório atual aos caminhos de inclusão
target_include_directories(ADC_DisplayC PRIVATE 
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/lib
    ${PICO_SDK_PATH}/src/rp2_common/hardware_pio/include
    ${CMAKE_CURRENT_LIST_DIR}/generated
)

pico_add_extra_outputs(ADC_DisplayC)

