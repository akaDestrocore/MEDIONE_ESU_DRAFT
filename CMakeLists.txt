cmake_minimum_required(VERSION 3.22)

project(esu_stm32f407 C CXX ASM)

include(cmake/gcc-arm-none-eabi.cmake)
enable_language(C CXX ASM)

# ============================================================
# Git hash
# ============================================================
execute_process(
    COMMAND git rev-parse --short=8 HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(NOT GIT_COMMIT_HASH)
    set(GIT_COMMIT_HASH "00000000")
endif()

message(STATUS "Git hash: ${GIT_COMMIT_HASH}")

# ============================================================
# Python
# ============================================================
find_program(PYTHON python3 REQUIRED)

set(VENV_DIR    ${CMAKE_SOURCE_DIR}/.venv)
set(VENV_PYTHON ${VENV_DIR}/bin/python)

if(NOT EXISTS ${VENV_PYTHON})
    message(STATUS "Creating Python virtual environment...")

    execute_process(
        COMMAND ${PYTHON} -m venv ${VENV_DIR}
        RESULT_VARIABLE VENV_RESULT
    )

    if(NOT VENV_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to create virtual environment")
    endif()

    message(STATUS "Installing Python dependencies...")

    execute_process(COMMAND ${VENV_PYTHON} -m pip install --upgrade pip)

    execute_process(
        COMMAND ${VENV_PYTHON} -m pip install -r ${CMAKE_SOURCE_DIR}/requirements.txt
        RESULT_VARIABLE PIP_RESULT
    )

    if(NOT PIP_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to install Python dependencies")
    endif()
endif()

execute_process(
    COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/scripts/tools/find_stm32cubeclt.py ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE _clt_exit
)

if(NOT _clt_exit EQUAL 0)
    message(FATAL_ERROR "find_stm32cubeclt.py failed (exit ${_clt_exit})")
endif()

set(PYTHON ${VENV_PYTHON})

# ============================================================
# Memory layout
# ============================================================
set(APP_OFFSET 0x4000)
set(APP_BASE   0x08004000)

# ============================================================
# Output files
# ============================================================
set(APP_MAP         ${CMAKE_BINARY_DIR}/app.map)
set(APP_BIN_RAW     ${CMAKE_BINARY_DIR}/app_raw.bin)
set(APP_BIN_PATCHED ${CMAKE_BINARY_DIR}/app_patched.bin)

set(BL_MAP          ${CMAKE_BINARY_DIR}/bootloader.map)
set(BL_BIN          ${CMAKE_BINARY_DIR}/bootloader.bin)

set(MERGED_BIN      ${CMAKE_BINARY_DIR}/firmware.bin)

# ============================================================
# MCU FLAGS
# ============================================================
set(CPU_FLAGS
    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
)

add_library(mcu INTERFACE)

target_compile_options(mcu INTERFACE
    ${CPU_FLAGS}
    -Wall -Wextra -Wno-unused-parameter
    -ffunction-sections -fdata-sections
    -fno-builtin
    $<$<CONFIG:Debug>:-O0 -g3>
    $<$<CONFIG:Release>:-Os>
)

target_link_options(mcu INTERFACE
    ${CPU_FLAGS}
    -Wl,--gc-sections
    -Wl,--print-memory-usage
)

target_compile_definitions(mcu INTERFACE
    STM32F407xx
    GIT_COMMIT_HASH="${GIT_COMMIT_HASH}"
)

target_include_directories(mcu INTERFACE
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
)

# ============================================================
# HAL DRIVER
# ============================================================
set(HAL_PATH ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src)

add_library(stm32f4_hal STATIC
    ${HAL_PATH}/stm32f4xx_hal.c
    ${HAL_PATH}/stm32f4xx_hal_cortex.c
    ${HAL_PATH}/stm32f4xx_hal_dma.c
    ${HAL_PATH}/stm32f4xx_hal_dma_ex.c
    ${HAL_PATH}/stm32f4xx_hal_exti.c
    ${HAL_PATH}/stm32f4xx_hal_flash.c
    ${HAL_PATH}/stm32f4xx_hal_flash_ex.c
    ${HAL_PATH}/stm32f4xx_hal_gpio.c
    ${HAL_PATH}/stm32f4xx_hal_pwr.c
    ${HAL_PATH}/stm32f4xx_hal_pwr_ex.c
    ${HAL_PATH}/stm32f4xx_hal_rcc.c
    ${HAL_PATH}/stm32f4xx_hal_rcc_ex.c
    ${HAL_PATH}/stm32f4xx_hal_crc.c
    ${HAL_PATH}/stm32f4xx_hal_uart.c
)

target_include_directories(stm32f4_hal
    PUBLIC
        ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Inc
)

target_compile_definitions(stm32f4_hal
    PUBLIC
        USE_HAL_DRIVER
)

target_link_libraries(stm32f4_hal PUBLIC mcu)

# ============================================================
# APPLICATION
# ============================================================
add_executable(app.elf)

target_sources(app.elf PRIVATE
    core/startup/startup_stm32f407vgtx.s
    core/src/system_stm32f4xx.c
    core/src/sysmem.c
    core/src/syscalls.c
    core/src/stm32f4xx_it.c
    core/src/main.c
    core/src/image_header.c
    common/src/image.c
    common/src/crc.c
    common/src/flash.c
)

target_include_directories(app.elf PRIVATE
    ${CMAKE_SOURCE_DIR}/core/inc
    ${CMAKE_SOURCE_DIR}/common/inc
)

target_link_options(app.elf PRIVATE
    -T${CMAKE_SOURCE_DIR}/linker/app.ld
    -Wl,-Map=${APP_MAP}
    --specs=nano.specs
    --specs=nosys.specs
)

target_link_libraries(app.elf PRIVATE stm32f4_hal mcu)

# ------------------------------------------------------------
# RAW BIN
# ------------------------------------------------------------

add_custom_command(TARGET app.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:app.elf> ${APP_BIN_RAW}
)

# ------------------------------------------------------------
# MEMAP
# ------------------------------------------------------------
add_custom_command(TARGET app.elf POST_BUILD
    COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/scripts/tools/memap.py -t GCC_ARM ${APP_MAP} -d 3
    COMMENT "Memory usage analysis"
)

add_custom_command(TARGET app.elf POST_BUILD
    COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/scripts/patch_image_header.py
            ${APP_BIN_RAW}
            ${APP_BIN_PATCHED}
            --base-addr ${APP_BASE}
)

# ============================================================
# BOOTLOADER (NO HAL)
# ============================================================
add_executable(bootloader.elf)

target_sources(bootloader.elf PRIVATE
    bootloader/startup/startup_stm32f407vgtx.s
    bootloader/src/main.c
    bootloader/src/system_stm32f4xx.c
)

target_include_directories(bootloader.elf PRIVATE
    ${CMAKE_SOURCE_DIR}/bootloader/inc
)

target_link_options(bootloader.elf PRIVATE
    -T${CMAKE_SOURCE_DIR}/linker/bootloader.ld
    -Wl,-Map=${BL_MAP}
    --specs=nano.specs
    --specs=nosys.specs
)

target_link_libraries(bootloader.elf PRIVATE mcu)

add_custom_command(TARGET bootloader.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:bootloader.elf> ${BL_BIN}
    COMMENT "Generating bootloader binary"
)

# ============================================================
# MERGE
# ============================================================
add_dependencies(app.elf bootloader.elf)

add_custom_command(TARGET app.elf POST_BUILD
    COMMAND ${PYTHON}
            ${CMAKE_SOURCE_DIR}/scripts/merge_bootloader.py
            ${BL_BIN}
            ${APP_BIN_PATCHED}
            ${MERGED_BIN}
            --offset ${APP_OFFSET}
    COMMENT "Merging bootloader + application"
)

# ============================================================
# FLASH
# ============================================================
add_custom_target(flash
    DEPENDS app.elf
    COMMAND STM32_Programmer_CLI
        -c port=swd mode=UR
        -d ${MERGED_BIN} 0x08000000
        -v -hardRst
)

add_custom_target(flash_app
    DEPENDS app.elf
    COMMAND STM32_Programmer_CLI
        -c port=swd mode=UR
        -d ${APP_BIN_PATCHED} ${APP_BASE}
        -v -hardRst
)