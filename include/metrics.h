#ifndef METRICS_H
#define METRICS_H

#include "common.h"

/* Вычисляет PSNR между исходным и восстановленным изображением. */
double psnr_metric(const Image *a, const Image *b);

/* Вычисляет SSIM между исходным и восстановленным изображением. */
double ssim_metric(const Image *a, const Image *b);

#endif
