# CMakeLists.txt — builds the SENTINEL C++ core
# * As an Android shared library (via NDK + Gradle externalNativeBuild)
# * Or as a desktop test executable (sentinel_core_tests) for rapid iteration
cmake_minimum_required(VERSION 3.22.1)
project("sentinel_core" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SENTINEL_SOURCES
    src/Graph.cpp
    src/CityMap.cpp
    src/Vehicle.cpp
    src/Routing.cpp
    src/Signals.cpp
    src/Dispatch.cpp
    src/ConfigManager.cpp
    src/SimulationEngine.cpp)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

if(ANDROID)
    # ---- Android native library with JNI bridge ----
    add_library(sentinel SHARED ${SENTINEL_SOURCES} jni_bridge.cpp)
    target_link_libraries(sentinel log android)
else()
    # ---- Desktop test harness (no JNI, no SFML required) ----
    add_executable(sentinel_core_tests ${SENTINEL_SOURCES} desktop_main.cpp)
endif()
