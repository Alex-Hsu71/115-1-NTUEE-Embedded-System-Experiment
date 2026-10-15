cmake_minimum_required(VERSION 3.22)

project(WiFi_Client_Server C ASM)

set(STM32CUBE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../STM32CubeL4" CACHE PATH "Path to the STM32CubeL4 package")
set(BSP_ROOT "${STM32CUBE_ROOT}/Drivers/BSP/B-L475E-IOT01" CACHE PATH "Path to the B-L475E-IOT01A BSP")

set(MCU_FLAGS
    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
)

set(PROJECT_SOURCES
    Src/main.c
    Src/stm32l4xx_it.c
    Src/system_stm32l4xx.c
    STM32CubeIDE/User/syscalls.c
    STM32CubeIDE/User/sysmem.c
    STM32CubeIDE/Startup/startup_stm32l475vgtx.s

    ${STM32CUBE_ROOT}/Projects/B-L475E-IOT01A/Applications/WiFi/Common/Src/es_wifi.c
    ${STM32CUBE_ROOT}/Projects/B-L475E-IOT01A/Applications/WiFi/Common/Src/es_wifi_io.c
    ${STM32CUBE_ROOT}/Projects/B-L475E-IOT01A/Applications/WiFi/Common/Src/wifi.c

    ${BSP_ROOT}/stm32l475e_iot01.c
    ${BSP_ROOT}/stm32l475e_iot01_accelero.c
    ${BSP_ROOT}/stm32l475e_iot01_gyro.c
    ${STM32CUBE_ROOT}/Drivers/BSP/Components/lsm6dsl/lsm6dsl.c

    ${STM32CUBE_ROOT}/Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal.c
    ${STM32CUBE_ROOT}/Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_cortex.c
    ${STM32CUBE_ROOT}/Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_dma.c
    ${STM32CUBE_ROOT}/Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_flash.c
    ${STM32CUBE_ROOT}/Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_flash_ex.c
    ${STM32CUBE_ROOT}/Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_gpio.c
    ${STM32CUBE_ROOT}/Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_i2c.c
    ${STM32CUBE_ROOT}/Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_i2c_ex.c
    ${STM32CUBE_ROOT}/Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_pwr.c
    ${STM32CUBE_ROOT}/Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_pwr_ex.c
    ${STM32CUBE_ROOT}/Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_rcc.c
    ${STM32CUBE_ROOT}/Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_rcc_ex.c
    ${STM32CUBE_ROOT}/Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_spi.c
    ${STM32CUBE_ROOT}/Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_spi_ex.c
    ${STM32CUBE_ROOT}/Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_uart.c
    ${STM32CUBE_ROOT}/Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_uart_ex.c
)

add_executable(${PROJECT_NAME}.elf ${PROJECT_SOURCES})

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    Inc
    ${STM32CUBE_ROOT}/Projects/B-L475E-IOT01A/Applications/WiFi/Common/Inc
    ${STM32CUBE_ROOT}/Drivers/CMSIS/Include
    ${STM32CUBE_ROOT}/Drivers/CMSIS/Device/ST/STM32L4xx/Include
    ${STM32CUBE_ROOT}/Drivers/STM32L4xx_HAL_Driver/Inc
    ${BSP_ROOT}
    ${STM32CUBE_ROOT}/Drivers/BSP/Components/Common
    ${STM32CUBE_ROOT}/Drivers/BSP/Components/hts221
    ${STM32CUBE_ROOT}/Drivers/BSP/Components/lis3mdl
    ${STM32CUBE_ROOT}/Drivers/BSP/Components/lps22hb
    ${STM32CUBE_ROOT}/Drivers/BSP/Components/lsm6dsl
)

target_compile_definitions(${PROJECT_NAME}.elf PRIVATE
    USE_HAL_DRIVER
    USE_STM32L475E_IOT01
    STM32L475xx
)

target_compile_options(${PROJECT_NAME}.elf PRIVATE
    ${MCU_FLAGS}
    -ffunction-sections
    -fdata-sections
    -Wall
)

target_link_options(${PROJECT_NAME}.elf PRIVATE
    ${MCU_FLAGS}
    -T${CMAKE_CURRENT_SOURCE_DIR}/STM32L475VGTX_FLASH.ld
    -Wl,--gc-sections
    -u _printf_float
    --specs=nano.specs
    --specs=nosys.specs
)

set_target_properties(${PROJECT_NAME}.elf PROPERTIES
    OUTPUT_NAME ${PROJECT_NAME}
    SUFFIX ".elf"
)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMAND ${CMAKE_OBJCOPY} -O ihex
            $<TARGET_FILE:${PROJECT_NAME}.elf>
            ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:${PROJECT_NAME}.elf>
            ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bin
)