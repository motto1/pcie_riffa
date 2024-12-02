cmake_minimum_required(VERSION 3.16)

project(pice_dll LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets)

# 添加riffa源文件
set(RIFFA_SOURCES
    riffa.cpp
    riffa.h
    riffa_driver.h
)

# 设置RIFFA库的路径
set(RIFFA_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib") 
set(RIFFA_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include") 

# 构建DLL库
add_library(pice_dll SHARED
    pice_dll_global.h
    pice_dll.cpp
    pice_dll.h
    ${RIFFA_SOURCES}
)

target_include_directories(pice_dll PRIVATE 
    ${RIFFA_INCLUDE_DIR}
)

target_link_directories(pice_dll PRIVATE 
    ${RIFFA_LIB_DIR}
)

target_link_libraries(pice_dll PRIVATE 
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    setupapi
    riffa
)

target_compile_definitions(pice_dll PRIVATE 
    PICE_DLL_LIB 
    RIFFA_EXPORTS
)

# 添加测试程序
add_executable(test_pice_dll WIN32
    test_main.cpp
    testwindow.cpp
    testwindow.h
    testwindow.ui
)

target_link_libraries(test_pice_dll PRIVATE
    pice_dll
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
)
