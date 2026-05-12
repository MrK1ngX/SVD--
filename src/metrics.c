#include "metrics.h"

#include <math.h>
#include <stddef.h>

double psnr_metric(const Image *a, const Image *b) {
    double mse = 0.0;
    int n = a->width * a->height * a->channels;
    int i;

    /* Считаем среднеквадратичную ошибку по всем каналам и пикселям. */
    for (i = 0; i < n; ++i) {
        double d = (double)a->data[i] - (double)b->data[i];
        mse += d * d;
    }
    mse /= n;

    /* Если изображения совпали почти идеально, возвращаем очень большое значение. */
    if (mse <= 1e-12) return 999.0;
    return 10.0 * log10((255.0 * 255.0) / mse);
}

static double ssim_metric_channel(const Image *a, const Image *b, int channel) {
    const double C1 = (0.01 * 255.0) * (0.01 * 255.0);
    const double C2 = (0.03 * 255.0) * (0.03 * 255.0);
    int n = a->width * a->height;
    double mx = 0.0, my = 0.0, vx = 0.0, vy = 0.0, cxy = 0.0;
    int x, y;

    /* Сначала считаем средние значения для выбранного канала. */
    for (y = 0; y < a->height; ++y) {
        for (x = 0; x < a->width; ++x) {
            size_t idx = ((size_t)y * a->width + x) * a->channels + channel;
            mx += a->data[idx];
            my += b->data[idx];
        }
    }
    mx /= n;
    my /= n;

    /* Потом считаем дисперсии и ковариацию. */
    for (y = 0; y < a->height; ++y) {
        for (x = 0; x < a->width; ++x) {
            size_t idx = ((size_t)y * a->width + x) * a->channels + channel;
            double dx = a->data[idx] - mx;
            double dy = b->data[idx] - my;
            vx += dx * dx;
            vy += dy * dy;
            cxy += dx * dy;
        }
    }
    vx /= n;
    vy /= n;
    cxy /= n;

    return ((2.0 * mx * my + C1) * (2.0 * cxy + C2)) /
           ((mx * mx + my * my + C1) * (vx + vy + C2));
}

double ssim_metric(const Image *a, const Image *b) {
    int c;
    double sum = 0.0;

    /* Итоговый SSIM усредняем по всем каналам. */
    for (c = 0; c < a->channels; ++c) sum += ssim_metric_channel(a, b, c);
    return sum / a->channels;
}
