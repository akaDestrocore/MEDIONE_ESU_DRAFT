set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Locate project root
get_filename_component(_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# Override
if(NOT CLT_PATH AND DEFINED ENV{CLT_PATH})
    set(CLT_PATH "$ENV{CLT_PATH}")
endif()

# Populate .vscode/settings.json
if(NOT CLT_PATH)
    find_program(_python3 NAMES python3 python)
    if(_python3)
        set(_discovery_script "${_root}/scripts/find_stm32cubeclt.py")
        if(EXISTS "${_discovery_script}")
            execute_process(
                COMMAND "${_python3}" "${_discovery_script}" "${_root}"
                RESULT_VARIABLE _script_exit
                ERROR_QUIET
            )
        endif()
    endif()
endif()

# Write STM32CubeCLT path to .vscode/settings.json
if(NOT CLT_PATH)
    set(_settings_json "${_root}/.vscode/settings.json")
    if(EXISTS "${_settings_json}")
        file(READ "${_settings_json}" _settings_content)
        string(JSON CLT_PATH ERROR_VARIABLE _json_err
               GET "${_settings_content}" "stm32.cltPath")
        if(_json_err)
            unset(CLT_PATH)
        endif()
    endif()
endif()

if(NOT CLT_PATH)
    if(CMAKE_HOST_WIN32)
        file(GLOB _candidates
            "C:/ST/STM32CubeCLT*"
            "$ENV{HOMEDRIVE}/ST/STM32CubeCLT*")
    else()
        file(GLOB _candidates
            "/opt/st/stm32cubeclt*"
            "/opt/ST/stm32cubeclt*"
            "/opt/st/STM32CubeCLT*"
            "/usr/local/st/stm32cubeclt*")
    endif()
    if(_candidates)
        list(SORT _candidates ORDER DESCENDING)
        list(GET  _candidates 0 CLT_PATH)
    endif()
endif()

# If not found anywhere
if(NOT CLT_PATH OR NOT EXISTS "${CLT_PATH}")
    message(FATAL_ERROR
        "STM32CubeCLT not found.\n"
        "  Linux  : expected under /opt/st/stm32cubeclt_*\n"
        "  Windows: expected under C:\\ST\\STM32CubeCLT_*\n"
        "\n"
        "Fix options:\n"
        "  cmake --preset Debug -DCLT_PATH=/path/to/stm32cubeclt\n"
        "  export CLT_PATH=/path/to/stm32cubeclt && cmake --preset Debug\n")
endif()

message(STATUS "STM32CubeCLT: ${CLT_PATH}")

set(_toolchain "${CLT_PATH}/GNU-tools-for-STM32/bin")

set(CMAKE_C_COMPILER   "${_toolchain}/arm-none-eabi-gcc")
set(CMAKE_CXX_COMPILER "${_toolchain}/arm-none-eabi-g++")
set(CMAKE_ASM_COMPILER "${_toolchain}/arm-none-eabi-gcc")
set(CMAKE_OBJCOPY      "${_toolchain}/arm-none-eabi-objcopy")
set(CMAKE_SIZE         "${_toolchain}/arm-none-eabi-size")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)