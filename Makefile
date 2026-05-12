# Компилятор проекта.
CC = gcc

# Общие флаги компиляции и путь к заголовочным файлам.
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -O2 -Iinclude

# Подключаем math library для cos, sqrt, log10, lround и других функций.
LDFLAGS = -lm

# Имена итоговых исполняемых файлов.
TARGET = image_compressor
SVD_TOOL = svd_to_bmp

# Основная программа собирается только из файлов src, без отдельной утилиты scripts/svd_to_bmp.c.
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)

# Основная цель по умолчанию собирает компрессор и C-утилиту восстановления SVD -> BMP.
all: $(TARGET) $(SVD_TOOL)

# Линковка готовой программы из объектных файлов.
$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

# Отдельная C-утилита для восстановления BMP из бинарного .svd.
$(SVD_TOOL): scripts/svd_to_bmp.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

# Правило сборки каждого .o из соответствующего .c.
src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Удаление результатов сборки.
clean:
	rm -f $(OBJ) $(TARGET) $(SVD_TOOL)

.PHONY: all clean
