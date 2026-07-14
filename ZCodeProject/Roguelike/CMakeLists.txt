# ============================================================================
# CMakeLists.txt —— Roguelike 地牢探险游戏
# 最低要求 CMake 3.10
# 用法:
#   mkdir build && cd build
#   cmake ..
#   cmake --build . --config Release
# ============================================================================

cmake_minimum_required(VERSION 3.10)
project(RoguelikeDungeon CXX)

# C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 源文件
set(SOURCES
    main.cpp
    Game.cpp
    Entity.cpp
    Map.cpp
    Skill.cpp
)

# 头文件（仅用于 IDE 显示，不参与编译）
set(HEADERS
    Game.h
    Entity.h
    Map.h
    Skill.h
    Career.h
)

# 生成可执行文件
add_executable(Roguelike ${SOURCES} ${HEADERS})

# 本项目仅依赖 Windows API，无需额外链接库
# （Windows 上 g++/MinGW 默认链接 kernel32/user32，含所需符号）

# MSVC 用户：需在属性中关闭 SDL 检查以允许 _kbhit/_getch
if(MSVC)
    target_compile_definitions(Roguelike PRIVATE _CRT_SECURE_NO_WARNINGS)
    # /utf-8 关键：源文件含中文注释，必须指定 UTF-8 编码否则编译报错
    target_compile_options(Roguelike PRIVATE /W3 /utf-8)
endif()

# MinGW/g++ 警告级别
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(Roguelike PRIVATE -Wall -Wextra)
endif()
