cmake_minimum_required(VERSION 3.22.1)
project(mfa_native LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(NATIVE_APP_GLUE "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue")
add_library(android_native_app_glue STATIC
    "${NATIVE_APP_GLUE}/android_native_app_glue.c")
target_include_directories(android_native_app_glue PUBLIC "${NATIVE_APP_GLUE}")
target_link_libraries(android_native_app_glue PUBLIC android log)

add_library(mfa_native SHARED
    src/native_game.cpp
    src/game_core.cpp
    src/model_asset.cpp)
target_include_directories(mfa_native PRIVATE include "${NATIVE_APP_GLUE}")
target_compile_options(mfa_native PRIVATE -O2 -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(mfa_native PRIVATE android log EGL GLESv2 android_native_app_glue)
