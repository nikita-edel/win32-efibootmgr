cmake_minimum_required(VERSION 3.16)
project(efibootmgr LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT WIN32)
    message(FATAL_ERROR "efibootmgr targets Windows only.")
endif()

add_executable(efibootmgr
    src/main.c
    src/efi_vars.c
    src/boot_entry.c
    src/dp.c
)

target_compile_definitions(efibootmgr PRIVATE
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
    _WIN32_WINNT=0x0602
    NTDDI_VERSION=0x06020000
)

target_link_libraries(efibootmgr PRIVATE kernel32)

if(MSVC)
    target_compile_options(efibootmgr PRIVATE /W4 /wd4100)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" AND
       (MINGW OR MSYS OR CMAKE_CROSSCOMPILING))
    target_compile_options(efibootmgr PRIVATE -Wall -Wextra -Wno-unused-parameter)
    target_link_options(efibootmgr PRIVATE -municode)
endif()
