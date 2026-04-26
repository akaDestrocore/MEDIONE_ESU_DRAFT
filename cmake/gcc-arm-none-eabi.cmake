set(CMAKE_SYSTEM_NAME               Generic)
set(CMAKE_SYSTEM_PROCESSOR          arm)

# Auto-discover the latest STM32CubeCLT installation
file(GLOB _CLT_CANDIDATES "/opt/st/stm32cubeclt_*")
list(SORT _CLT_CANDIDATES ORDER DESCENDING)
list(GET _CLT_CANDIDATES 0 _CLT_PATH)

if(NOT _CLT_PATH)
    message(FATAL_ERROR "No STM32CubeCLT installation found under /opt/st/")
endif()

message(STATUS "Using STM32CubeCLT: ${_CLT_PATH}")

# Some default GCC settings
# arm-none-eabi- must be part of path environment

set(TOOLCHAIN_PATH ${_CLT_PATH}/GNU-tools-for-STM32/bin)

set(CMAKE_C_COMPILER   ${TOOLCHAIN_PATH}/arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PATH}/arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PATH}/arm-none-eabi-gcc)

set(CMAKE_OBJCOPY ${TOOLCHAIN_PATH}/arm-none-eabi-objcopy)
set(CMAKE_SIZE    ${TOOLCHAIN_PATH}/arm-none-eabi-size)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)