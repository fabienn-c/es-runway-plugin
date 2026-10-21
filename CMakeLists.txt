cmake_minimum_required(VERSION 3.20)
project(ESRunwayPlugin VERSION 0.3.0 LANGUAGES CXX)

# EuroScope is a 32-bit application: a 64-bit DLL will not load.
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Configure for 32-bit: cmake -S . -B build -A Win32")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static MSVC runtime so controllers don't need a VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(ESRunwayPlugin SHARED
    src/Main.cpp
    src/RunwayPlugin.cpp
    src/RunwayDialog.cpp
    src/WindowProbe.cpp
    src/Log.cpp
)

# SYSTEM keeps warnings from the EuroScope SDK header out of our build output.
target_include_directories(ESRunwayPlugin SYSTEM PRIVATE sdk)
target_include_directories(ESRunwayPlugin PRIVATE src)
target_compile_definitions(ESRunwayPlugin PRIVATE
    WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS
    PLUGIN_VERSION="${PROJECT_VERSION}"
)
target_compile_options(ESRunwayPlugin PRIVATE /W4 /utf-8)
target_link_libraries(ESRunwayPlugin PRIVATE ${CMAKE_SOURCE_DIR}/sdk/EuroScopePlugInDll.lib)
