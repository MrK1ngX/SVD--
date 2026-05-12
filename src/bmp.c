#include "bmp.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Структуры ниже повторяют заголовки BMP-файла.
 * Упаковка нужна, чтобы расположение полей совпадало с форматом на диске.
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} BMPFileHeader;

typedef struct {
    uint32_t biSize;
    int32_t biWidth;
    int32_t biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t biXPelsPerMeter;
    int32_t biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} BMPInfoHeader;
#pragma pack(pop)

int read_bmp(const char *filename, Image *img) {
    FILE *fp = fopen(filename, "rb");
    BMPFileHeader fh;
    BMPInfoHeader ih;
    int width, height, top_down;

    if (!fp || !img) return 0;

    /* Считываем служебные заголовки файла. */
    if (fread(&fh, sizeof(fh), 1, fp) != 1) {
        fclose(fp);
        return 0;
    }
    if (fread(&ih, sizeof(ih), 1, fp) != 1) {
        fclose(fp);
        return 0;
    }

    /* Поддерживаем только обычный несжатый BMP. */
    if (fh.bfType != 0x4D42 || ih.biCompression != 0) {
        fclose(fp);
        return 0;
    }

    width = ih.biWidth;
    height = ih.biHeight < 0 ? -ih.biHeight : ih.biHeight;
    top_down = ih.biHeight < 0;

    if (width <= 0 || height <= 0) {
        fclose(fp);
        return 0;
    }

    img->width = width;
    img->height = height;
    img->channels = 3;
    img->data = (unsigned char *)malloc((size_t)width * height * 3);
    if (!img->data) {
        fclose(fp);
        return 0;
    }

    /* Переходим к началу пиксельных данных. */
    fseek(fp, fh.bfOffBits, SEEK_SET);

    if (ih.biBitCount == 24) {
        /* 24-битный BMP хранит пиксели в порядке BGR и с выравниванием строк. */
        int row_size = ((width * 3 + 3) / 4) * 4;
        unsigned char *row = (unsigned char *)malloc((size_t)row_size);
        int y, x;
        if (!row) {
            free_image(img);
            fclose(fp);
            return 0;
        }

        for (y = 0; y < height; ++y) {
            /* В BMP строки часто идут снизу вверх, поэтому пересчитываем индекс. */
            int yy = top_down ? y : (height - 1 - y);
            if (fread(row, 1, row_size, fp) != (size_t)row_size) {
                free(row);
                free_image(img);
                fclose(fp);
                return 0;
            }
            for (x = 0; x < width; ++x) {
                unsigned char b = row[x * 3 + 0];
                unsigned char g = row[x * 3 + 1];
                unsigned char r = row[x * 3 + 2];
                size_t idx = ((size_t)yy * width + x) * 3;
                img->data[idx + 0] = r;
                img->data[idx + 1] = g;
                img->data[idx + 2] = b;
            }
        }
        free(row);
    } else if (ih.biBitCount == 8) {
        /* 8-битный BMP читает индекс цвета из палитры. */
        int row_size = ((width + 3) / 4) * 4;
        unsigned char palette[256][4];
        unsigned char *row = (unsigned char *)malloc((size_t)row_size);
        int y, x;
        long palette_pos = sizeof(BMPFileHeader) + ih.biSize;
        if (!row) {
            free_image(img);
            fclose(fp);
            return 0;
        }

        if (fh.bfOffBits < (uint32_t)(palette_pos + 256 * 4)) {
            free(row);
            free_image(img);
            fclose(fp);
            return 0;
        }

        fseek(fp, palette_pos, SEEK_SET);
        if (fread(palette, 4, 256, fp) != 256) {
            free(row);
            free_image(img);
            fclose(fp);
            return 0;
        }
        fseek(fp, fh.bfOffBits, SEEK_SET);

        for (y = 0; y < height; ++y) {
            int yy = top_down ? y : (height - 1 - y);
            if (fread(row, 1, row_size, fp) != (size_t)row_size) {
                free(row);
                free_image(img);
                fclose(fp);
                return 0;
            }
            for (x = 0; x < width; ++x) {
                unsigned char p = row[x];
                size_t idx = ((size_t)yy * width + x) * 3;
                img->data[idx + 0] = palette[p][2];
                img->data[idx + 1] = palette[p][1];
                img->data[idx + 2] = palette[p][0];
            }
        }
        free(row);
    } else {
        /* Остальные глубины цвета здесь не поддерживаются. */
        free_image(img);
        fclose(fp);
        return 0;
    }

    fclose(fp);
    return 1;
}

int write_bmp(const char *filename, const Image *img) {
    FILE *fp = fopen(filename, "wb");
    BMPFileHeader fh;
    BMPInfoHeader ih;
    int row_size;
    int image_size;
    int file_size;
    unsigned char *row;
    int y, x;

    if (!fp) return 0;
    if (!img || !img->data || img->channels != 3) {
        fclose(fp);
        return 0;
    }

    /* Считаем размеры с учётом выравнивания каждой строки до 4 байт. */
    row_size = ((img->width * 3 + 3) / 4) * 4;
    image_size = row_size * img->height;
    file_size = (int)(sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + image_size);

    fh.bfType = 0x4D42;
    fh.bfSize = (uint32_t)file_size;
    fh.bfReserved1 = 0;
    fh.bfReserved2 = 0;
    fh.bfOffBits = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader);

    ih.biSize = sizeof(BMPInfoHeader);
    ih.biWidth = img->width;
    ih.biHeight = img->height;
    ih.biPlanes = 1;
    ih.biBitCount = 24;
    ih.biCompression = 0;
    ih.biSizeImage = (uint32_t)image_size;
    ih.biXPelsPerMeter = 2835;
    ih.biYPelsPerMeter = 2835;
    ih.biClrUsed = 0;
    ih.biClrImportant = 0;

    fwrite(&fh, sizeof(fh), 1, fp);
    fwrite(&ih, sizeof(ih), 1, fp);

    row = (unsigned char *)calloc((size_t)row_size, 1);
    if (!row) {
        fclose(fp);
        return 0;
    }

    /* BMP пишет строки снизу вверх, поэтому идём от последней к первой. */
    for (y = img->height - 1; y >= 0; --y) {
        memset(row, 0, (size_t)row_size);
        for (x = 0; x < img->width; ++x) {
            size_t idx = ((size_t)y * img->width + x) * 3;
            row[x * 3 + 0] = img->data[idx + 2];
            row[x * 3 + 1] = img->data[idx + 1];
            row[x * 3 + 2] = img->data[idx + 0];
        }
        fwrite(row, 1, (size_t)row_size, fp);
    }

    free(row);
    fclose(fp);
    return 1;
}
