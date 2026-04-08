cmake_minimum_required(VERSION 3.16)

project(SystemMonitorDemo VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

qt_add_executable(SystemMonitorDemo
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    systemstats.cpp
    systemstats.h
    processtable.cpp
    processtable.h
    linegraphwidget.cpp
    linegraphwidget.h
    bargraphwidget.cpp
    bargraphwidget.h
    resources.qrc
)

target_link_libraries(SystemMonitorDemo PRIVATE Qt6::Widgets)
