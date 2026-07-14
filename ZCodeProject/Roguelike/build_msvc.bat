@echo off
REM ============================================================================
REM build_msvc.bat -- Build the Roguelike game with MSVC (cl.exe)
REM Usage: double-click or run from command line
REM Requires: Visual Studio 2022 (or Build Tools) with C++ workload installed.
REM ============================================================================
setlocal

REM Initialize MSVC build environment
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

REM Switch to script directory
cd /d "%~dp0"

REM Compile
REM   /utf-8                 -- REQUIRED: source files contain Chinese comments in UTF-8
REM   /EHsc                  -- enable C++ exception handling
REM   /std:c++17             -- use C++17 standard
REM   /W3                    -- warning level 3
REM   /D_CRT_SECURE_NO_WARNINGS -- silence _getch/_kbhit security warnings
cl /EHsc /std:c++17 /utf-8 /W3 /D_CRT_SECURE_NO_WARNINGS /nologo ^
   main.cpp Game.cpp Entity.cpp Map.cpp Skill.cpp ^
   /Fe:Roguelike.exe

if %ERRORLEVEL% == 0 (
    echo.
    echo === Build succeeded: Roguelike.exe ===
) else (
    echo.
    echo === Build failed, error code %ERRORLEVEL% ===
)

endlocal
