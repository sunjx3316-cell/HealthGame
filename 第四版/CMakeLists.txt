cmake_minimum_required(VERSION 3.16)

project(HealthGame VERSION 0.1 LANGUAGES CXX)

# 设置 C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 开启 Qt 自动构建工具
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# 查找 Qt 包（优先找 Qt6，如果没有则找 Qt5）
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets Network)

# 添加可执行文件及其源代码
add_executable(HealthGame
    main.cpp
    mainwindow.cpp
    mainwindow.h
    Models.h
    Database.cpp
    Database.h
    GameEngine.cpp
    GameEngine.h
    ResourceManager.h
    ResourceManager.cpp
    StartupDialog.h
    StartupDialog.cpp
    resources.qrc
)

# 链接 Qt 库
target_link_libraries(HealthGame PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
)

# 针对 Windows 系统，设置为 GUI 应用（隐藏控制台窗口）
if(WIN32)
    set_target_properties(HealthGame PROPERTIES WIN32_EXECUTABLE ON)
endif()
