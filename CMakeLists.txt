cmake_minimum_required(VERSION 3.10)

project(AndKittyModProject)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(INJECTOR_ONLY "Build only the injector executable" OFF)

add_compile_definitions(_USE_MATH_DEFINES)
add_compile_definitions(M_PI=3.14159265358979323846)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
add_compile_options(-fvisibility=hidden)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-include cstdint)
endif()

set(SOURCE_DIR ${CMAKE_SOURCE_DIR})

include_directories(
    ${SOURCE_DIR}
    ${SOURCE_DIR}/include
    ${SOURCE_DIR}/backends
)

# x86_64: 只编 injector（MuMu Houdini）；MyMenu/dobby 仍是 arm64
set(BUILD_MYMENU ON)
if(INJECTOR_ONLY OR (DEFINED ANDROID_ABI AND ANDROID_ABI STREQUAL "x86_64"))
    set(BUILD_MYMENU OFF)
endif()

if(BUILD_MYMENU)
    add_library(dobby STATIC IMPORTED)
    set_target_properties(dobby PROPERTIES IMPORTED_LOCATION
        ${SOURCE_DIR}/libdobby.a)

    set(PLUGIN_SOURCES
        main.cpp
        imgui.cpp
        imgui_draw.cpp
        imgui_widgets.cpp
        imgui_tables.cpp
        imgui_demo.cpp
        backends/imgui_impl_android.cpp
        backends/imgui_impl_opengl3.cpp
    )

    add_library(MyMenu SHARED ${PLUGIN_SOURCES})
    target_link_libraries(MyMenu
        dobby
        log
        GLESv3
        EGL
        android
        z
    )
endif()

if(EXISTS "${SOURCE_DIR}/injector.cpp")
    add_executable(injector injector.cpp)
    target_link_libraries(injector
        log
        dl
    )
    set_target_properties(injector PROPERTIES LINK_FLAGS "-static-libstdc++")
endif()

message(STATUS "Android ABI: ${ANDROID_ABI}")
message(STATUS "BUILD_MYMENU: ${BUILD_MYMENU}")
if(ANDROID_ABI STREQUAL "x86_64")
    message(STATUS "x86_64: building injector for MuMu Houdini (pair with ARM64 libMyMenu.so)")
elseif(ANDROID_ABI STREQUAL "arm64-v8a")
    message(STATUS "arm64-v8a: phone injector + libMyMenu.so")
endif()
