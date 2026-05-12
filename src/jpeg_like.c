#include "jpeg_like.h"

#include "metrics.h"

#include <math.h>
#include <stdlib.h>
#include <time.h>

/* Базовая таблица квантования, похожая на стандартную JPEG-таблицу яркости. */
static const int jpeg_qtable[8][8] = {
    {16, 11, 10, 16, 24, 40, 51, 61},
    {12, 12, 14, 19, 26, 58, 60, 55},
    {14, 13, 16, 24, 40, 57, 69, 56},
    {14, 17, 22, 29, 51, 87, 80, 62},
    {18, 22, 37, 56, 68,109,103, 77},
    {24, 35, 55, 64, 81,104,113, 92},
    {49, 64, 78, 87,103,121,120,101},
    {72, 92, 95, 98,112,100,103, 99}
};

static void build_scaled_qtable(int quality, int out[8][8]) {
    int scale, i, j;

    /* Ограничиваем quality и переводим его в множитель для таблицы. */
    if (quality < 1) quality = 1;
    if (quality > 100) quality = 100;
    scale = (quality < 50) ? (5000 / quality) : (200 - 2 * quality);

    for (i = 0; i < 8; ++i) {
        for (j = 0; j < 8; ++j) {
            int q = (jpeg_qtable[i][j] * scale + 50) / 100;
            out[i][j] = clamp_int(q, 1, 255);
        }
    }
}

static void dct8(double in[8][8], double out[8][8]) {
    int u, v, x, y;

    /* Прямое DCT для блока 8x8. */
    for (u = 0; u < 8; ++u) {
        for (v = 0; v < 8; ++v) {
            double cu = (u == 0) ? 1.0 / sqrt(2.0) : 1.0;
            double cv = (v == 0) ? 1.0 / sqrt(2.0) : 1.0;
            double sum = 0.0;
            for (x = 0; x < 8; ++x) {
                for (y = 0; y < 8; ++y) {
                    sum += in[x][y] *
                           cos(((2 * x + 1) * u * M_PI) / 16.0) *
                           cos(((2 * y + 1) * v * M_PI) / 16.0);
                }
            }
            out[u][v] = 0.25 * cu * cv * sum;
        }
    }
}

static void idct8(double in[8][8], double out[8][8]) {
    int x, y, u, v;

    /* Обратное DCT возвращает восстановленный блок обратно в пиксели. */
    for (x = 0; x < 8; ++x) {
        for (y = 0; y < 8; ++y) {
            double sum = 0.0;
            for (u = 0; u < 8; ++u) {
                for (v = 0; v < 8; ++v) {
                    double cu = (u == 0) ? 1.0 / sqrt(2.0) : 1.0;
                    double cv = (v == 0) ? 1.0 / sqrt(2.0) : 1.0;
                    sum += cu * cv * in[u][v] *
                           cos(((2 * x + 1) * u * M_PI) / 16.0) *
                           cos(((2 * y + 1) * v * M_PI) / 16.0);
                }
            }
            out[x][y] = 0.25 * sum;
        }
    }
}

static int jpeg_like_compress_channel(const Image *img, int channel, int quality, unsigned char *dst, long long *nonzero_total) {
    int width = img->width;
    int height = img->height;
    int pw = (width + 7) / 8 * 8;
    int ph = (height + 7) / 8 * 8;
    double *padded = (double *)calloc((size_t)pw * ph, sizeof(double));
    double *recon = (double *)calloc((size_t)pw * ph, sizeof(double));
    int qtable[8][8];
    int bx, by, x, y;

    if (!padded || !recon) {
        free(padded);
        free(recon);
        return 0;
    }

    build_scaled_qtable(quality, qtable);

    /*
     * Расширяем канал до размера, кратного 8.
     * Недостающие пиксели добираем повторением края.
     */
    for (y = 0; y < ph; ++y) {
        for (x = 0; x < pw; ++x) {
            int sx = (x < width) ? x : (width - 1);
            int sy = (y < height) ? y : (height - 1);
            size_t idx = ((size_t)sy * width + sx) * img->channels + channel;
            padded[y * pw + x] = (double)img->data[idx] - 128.0;
        }
    }

    /* Обрабатываем изображение блоками 8x8, как в JPEG. */
    for (by = 0; by < ph; by += 8) {
        for (bx = 0; bx < pw; bx += 8) {
            double block[8][8], coeff[8][8], deq[8][8], rec[8][8];
            int qcoeff[8][8];

            for (y = 0; y < 8; ++y) {
                for (x = 0; x < 8; ++x) {
                    block[y][x] = padded[(by + y) * pw + (bx + x)];
                }
            }

            dct8(block, coeff);

            /*
             * Квантование — основной этап потерь.
             * Заодно считаем число ненулевых коэффициентов как грубую оценку объёма.
             */
            for (y = 0; y < 8; ++y) {
                for (x = 0; x < 8; ++x) {
                    qcoeff[y][x] = (int)llround(coeff[y][x] / qtable[y][x]);
                    if (qcoeff[y][x] != 0) (*nonzero_total)++;
                    deq[y][x] = (double)(qcoeff[y][x] * qtable[y][x]);
                }
            }

            idct8(deq, rec);
            for (y = 0; y < 8; ++y) {
                for (x = 0; x < 8; ++x) {
                    recon[(by + y) * pw + (bx + x)] = rec[y][x] + 128.0;
                }
            }
        }
    }

    /* Копируем восстановленные значения обратно в реальный размер изображения. */
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            size_t idx = ((size_t)y * width + x) * img->channels + channel;
            dst[idx] = (unsigned char)clamp_int((int)llround(recon[y * pw + x]), 0, 255);
        }
    }

    free(padded);
    free(recon);
    return 1;
}

Image jpeg_like_compress(const Image *img, int quality, RunStats *stats) {
    long long nonzero = 0;
    int c;
    Image out = {0, 0, 0, NULL};
    int block_count;
    clock_t t0 = clock();

    out.width = img->width;
    out.height = img->height;
    out.channels = img->channels;
    out.data = (unsigned char *)malloc((size_t)img->width * img->height * img->channels);
    if (!out.data) return out;

    /* Сжимаем каждый канал отдельно. */
    for (c = 0; c < img->channels; ++c) {
        if (!jpeg_like_compress_channel(img, c, quality, out.data, &nonzero)) {
            free_image(&out);
            return out;
        }
    }

    if (stats) {
        stats->seconds = (double)(clock() - t0) / CLOCKS_PER_SEC;
        stats->psnr = psnr_metric(img, &out);
        stats->ssim = ssim_metric(img, &out);

        /* Оценка степени сжатия здесь приблизительная, но удобна для сравнения запусков. */
        block_count = ((img->width + 7) / 8) * ((img->height + 7) / 8);
        {
            double original_bytes = (double)(img->width * img->height * img->channels);
            double stored_bytes = nonzero * 2.0 + (double)block_count * 8.0 * img->channels;
            stats->ratio = stored_bytes > 0.0 ? original_bytes / stored_bytes : 0.0;
        }
    }

    return out;
}
