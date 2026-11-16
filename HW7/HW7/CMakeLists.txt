cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME HW7)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# The BSP talks to the LSM6DSL over I2C2, but I2C2 is intentionally NOT enabled
# in CubeMX (the BSP owns it). Enable the HAL I2C module here for every target
# (incl. the STM32_Drivers object library) so regenerating code keeps it.
target_compile_definitions(stm32cubemx INTERFACE HAL_I2C_MODULE_ENABLED HAL_SPI_MODULE_ENABLED)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/DSP/Lib/GCC
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    App/Src/app.c
    App/Src/dsp_filter.c
    Drivers/BSP/B-L475E-IOT01/stm32l475e_iot01.c
    Drivers/BSP/B-L475E-IOT01/stm32l475e_iot01_accelero.c
    Drivers/BSP/Components/lsm6dsl/lsm6dsl.c
    # ISM43362 WiFi (SPI3) - not enabled in CubeMX, the driver owns SPI3/EXTI1
    Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_spi.c
    Drivers/STM32L4xx_HAL_Driver/Src/stm32l4xx_hal_spi_ex.c
    Drivers/WiFi/Src/es_wifi.c
    Drivers/WiFi/Src/es_wifi_io.c
    Drivers/WiFi/Src/wifi.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    App/Inc
    Drivers/BSP/B-L475E-IOT01
    Drivers/CMSIS/DSP/Include
    Drivers/WiFi/Inc
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
    ARM_MATH_CM4
)

# printf/snprintf with %f under newlib-nano
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE -u _printf_float)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
    arm_cortexM4lf_math
)
