cmake_minimum_required(VERSION 3.16)
project(shift2d VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Настройки Qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Поиск Qt5
find_package(Qt5 COMPONENTS Core Widgets REQUIRED)

# Поиск segyio
find_package(segyio REQUIRED)

# Исполняемый файл
add_executable(shift2d
    main.cpp
    main_window.cpp
    main_window.hpp
    seismic_view.cpp
    seismic_view.hpp
    segy_reader.cpp
    segy_reader.hpp
    segy_writer.cpp
    segy_writer.hpp
    color_schemes.cpp
    color_schemes.hpp
    types.hpp
    segy_load_dialog.cpp
    segy_load_dialog.hpp
    statics_load_dialog.cpp
    statics_load_dialog.hpp
    horizon_load_dialog.cpp
    horizon_load_dialog.hpp
    load_preview_widgets.cpp
    load_preview_widgets.hpp
)

# Линковка
target_link_libraries(shift2d
    Qt5::Core
    Qt5::Widgets
    segyio::segyio
)

# Установка
install(TARGETS shift2d
    RUNTIME DESTINATION bin
)
