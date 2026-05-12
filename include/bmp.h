#ifndef BMP_H
#define BMP_H

#include "common.h"

/* Загружает BMP-файл в структуру Image. */
int read_bmp(const char *filename, Image *img);

/* Сохраняет изображение в 24-битный BMP. */
int write_bmp(const char *filename, const Image *img);

#endif
