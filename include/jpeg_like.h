#ifndef JPEG_LIKE_H
#define JPEG_LIKE_H

#include "common.h"

/* Выполняет JPEG-подобное сжатие с качеством 1..100. */
Image jpeg_like_compress(const Image *img, int quality, RunStats *stats);

#endif
