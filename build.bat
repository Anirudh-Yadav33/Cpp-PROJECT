@echo off
echo ========================================================
echo Building Project Evaluation System Server in C++ and SQL
echo ========================================================

if not exist sqlite3.o (
    echo Compiling sqlite3.c using GCC C compiler...
    gcc -c sqlite3.c -o sqlite3.o
)

echo Compiling C++ application files using G++...
g++ -std=c++14 -O2 main.cpp http_server.cpp sql_database.cpp pdf_parser.cpp ai_evaluator.cpp chat_engine.cpp excel_exporter.cpp sqlite3.o -o server.exe -lws2_32

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================================
    echo BUILD SUCCESSFUL! Created server.exe
    echo ========================================================
) else (
    echo.
    echo [ERROR] Compilation failed.
    exit /b 1
)
