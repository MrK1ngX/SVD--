#ifndef JPEG_WRITER_H
#define JPEG_WRITER_H

#include "common.h"

/* Saves an RGB image as a real baseline JPEG file. Quality is 1..100. */
int write_jpeg(const char *filename, const Image *img, int quality);

#endif
