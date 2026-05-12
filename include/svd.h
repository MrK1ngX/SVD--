#ifndef SVD_H
#define SVD_H

#include "common.h"

/* Выполняет SVD-сжатие и возвращает восстановленное изображение в памяти. */
Image svd_compress(const Image *img, int k, RunStats *stats);

/* Выполняет SVD-сжатие, пишет бинарный .svd и возвращает восстановленное изображение. */
int svd_compress_to_file(const Image *img, int k, const char *filename, Image *out, RunStats *stats);

#endif
