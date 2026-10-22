cmake_minimum_required(VERSION 3.20)
project(ESRunwayPlugin VERSION 0.9.0 LANGUAGES CXX)

# EuroScope is a 32-bit application: a 64-bit DLL will not load.
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Configure for 32-bit: cmake -S . -B build -A Win32")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static MSVC runtime so controllers don't need a VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

# Runway logic without EuroScope dependencies, shared by the plugin and the tests.
add_library(RunwayCore STATIC
    src/core/Config.cpp
    src/core/LocalTime.cpp
    src/core/Metar.cpp
    src/core/Selector.cpp
)
target_include_directories(RunwayCore PUBLIC src/core)
target_link_libraries(RunwayCore PUBLIC nlohmann_json::nlohmann_json)
target_compile_definitions(RunwayCore PRIVATE _CRT_SECURE_NO_WARNINGS)
target_compile_options(RunwayCore PRIVATE /W4 /utf-8)

add_library(ESRunwayPlugin SHARED
    src/Main.cpp
    src/RunwayPlugin.cpp
    src/RunwayApplier.cpp
    src/RunwayDialog.cpp
    src/RunwayIcon.cpp
    src/RunwayScreen.cpp
    src/MetarFeed.cpp
    src/ScreenCapture.cpp
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
target_link_libraries(ESRunwayPlugin PRIVATE RunwayCore winhttp ${CMAKE_SOURCE_DIR}/sdk/EuroScopePlugInDll.lib)

enable_testing()
add_executable(CoreTests tests/CoreTests.cpp)
target_link_libraries(CoreTests PRIVATE RunwayCore)
add_test(NAME CoreTests COMMAND CoreTests ${CMAKE_SOURCE_DIR}/config/LEXX.json)
