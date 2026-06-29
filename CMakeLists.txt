cmake_minimum_required(VERSION 3.18)
project(shift2d VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Настройки Qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Поиск Qt5
find_package(Qt5 COMPONENTS Core Widgets REQUIRED)

# segyio: локальная копия или FetchContent с GitHub
include(FetchContent)
set(BUILD_PYTHON OFF)
set(BUILD_BIN OFF)
set(BUILD_TESTING OFF)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/segyio/CMakeLists.txt")
    add_subdirectory(segyio)
else()
    FetchContent_Declare(
        segyio
        GIT_REPOSITORY https://github.com/equinor/segyio.git
        GIT_TAG v1.9.14  # 1.x C API; v2.0.0-alpha ломает segy_reader/writer
    )
    FetchContent_MakeAvailable(segyio)
endif()

set(SHIFT2D_SOURCES
    src/main.cpp
    src/main_window.cpp
    src/seismic_view.cpp
    src/segy_reader.cpp
    src/segy_writer.cpp
    src/color_schemes.cpp
    src/segy_load_dialog.cpp
    src/statics_load_dialog.cpp
    src/horizon_load_dialog.cpp
    src/load_preview_widgets.cpp
)

set(SHIFT2D_HEADERS
    include/main_window.hpp
    include/seismic_view.hpp
    include/segy_reader.hpp
    include/segy_writer.hpp
    include/color_schemes.hpp
    include/types.hpp
    include/segy_load_dialog.hpp
    include/statics_load_dialog.hpp
    include/horizon_load_dialog.hpp
    include/load_preview_widgets.hpp
)

add_executable(shift2d ${SHIFT2D_SOURCES} ${SHIFT2D_HEADERS})

target_include_directories(shift2d
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(shift2d
    PRIVATE
        Qt5::Core
        Qt5::Widgets
        segyio
)

install(TARGETS shift2d
    RUNTIME DESTINATION bin
)
