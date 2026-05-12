@echo off
setlocal

REM Сборка проекта в Windows через gcc без make.
REM Подходит для MSYS2/MinGW, если gcc добавлен в PATH.

gcc src\main.c src\bmp.c src\cli.c src\common.c src\compare.c src\jpeg_like.c src\jpeg_writer.c src\metrics.c src\svd.c -Iinclude -Wall -Wextra -Wpedantic -std=c11 -O2 -lm -o image_compressor.exe
if errorlevel 1 (
    echo.
    echo Build failed: image_compressor.exe
    exit /b 1
)

gcc scripts\svd_to_bmp.c -Wall -Wextra -Wpedantic -std=c11 -O2 -lm -o svd_to_bmp.exe
if errorlevel 1 (
    echo.
    echo Build failed: svd_to_bmp.exe
    exit /b 1
)

echo.
echo Build completed: image_compressor.exe and svd_to_bmp.exe
exit /b 0
