set(CMAKE_SYSTEM_NAME      Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

if(NOT DEFINED MINGW_PREFIX)
    find_program(_CC  NAMES x86_64-w64-mingw32-gcc-posix
                            x86_64-w64-mingw32-gcc
                            x86_64-mingw32-gcc)
    find_program(_CXX NAMES x86_64-w64-mingw32-g++-posix
                            x86_64-w64-mingw32-g++
                            x86_64-mingw32-g++)
    find_program(_RC  NAMES x86_64-w64-mingw32-windres
                            x86_64-mingw32-windres)
else()
    find_program(_CC  NAMES "${MINGW_PREFIX}-gcc-posix" "${MINGW_PREFIX}-gcc")
    find_program(_CXX NAMES "${MINGW_PREFIX}-g++-posix" "${MINGW_PREFIX}-g++")
    find_program(_RC  NAMES "${MINGW_PREFIX}-windres")
endif()

if(NOT _CC)
    message(FATAL_ERROR
        "MinGW-w64 C compiler not found.\n"
        "Install it (e.g. `sudo pacman -S mingw-w64-gcc`) "
        "or set -DMINGW_PREFIX=<prefix>.")
endif()

set(CMAKE_C_COMPILER   "${_CC}"  CACHE PATH "C compiler")
set(CMAKE_CXX_COMPILER "${_CXX}" CACHE PATH "C++ compiler")
if(_RC)
    set(CMAKE_RC_COMPILER "${_RC}" CACHE PATH "RC compiler")
endif()

get_filename_component(_CC_DIR    "${_CC}" DIRECTORY)
get_filename_component(_CC_SYSROOT "${_CC_DIR}/.." ABSOLUTE)

if(EXISTS "${_CC_SYSROOT}/x86_64-w64-mingw32/include"
   OR EXISTS "${_CC_SYSROOT}/include")
    set(CMAKE_FIND_ROOT_PATH "${_CC_SYSROOT}" CACHE PATH "Sysroot")
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

add_link_options(-static-libgcc)
