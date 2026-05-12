#include "svd.h"

#include "metrics.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double *image_to_matrix_channel(const Image *img, int channel) {
    int x, y;
    double *m;

    /* Преобразуем один цветовой канал изображения в матрицу double. */
    if (!img || !img->data || channel < 0 || channel >= img->channels) return NULL;
    m = (double *)malloc((size_t)img->width * img->height * sizeof(double));
    if (!m) return NULL;

    for (y = 0; y < img->height; ++y) {
        for (x = 0; x < img->width; ++x) {
            size_t pixel = ((size_t)y * img->width + x) * img->channels;
            m[y * img->width + x] = (double)img->data[pixel + channel];
        }
    }
    return m;
}

static void write_matrix_to_channel(unsigned char *dst, const double *m, int width, int height, int channels, int channel) {
    int x, y;

    /* Возвращаем матрицу обратно в пиксели и зажимаем значения в диапазон 0..255. */
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            size_t pixel = ((size_t)y * width + x) * channels;
            int v = (int)llround(m[y * width + x]);
            dst[pixel + channel] = (unsigned char)clamp_int(v, 0, 255);
        }
    }
}

static void build_ata(const double *A, int m, int n, double *ATA) {
    int i, j, k;

    /* Строим матрицу A^T * A, по которой дальше ищем собственные значения. */
    for (i = 0; i < n; ++i) {
        for (j = i; j < n; ++j) {
            double sum = 0.0;
            for (k = 0; k < m; ++k) sum += A[k * n + i] * A[k * n + j];
            ATA[i * n + j] = ATA[j * n + i] = sum;
        }
    }
}

static void jacobi_eigen(double *A, int n, double *eigenvalues, double *V) {
    int i, j, iter;
    int max_iter = n * n * 50 + 100;
    double eps = 1e-8;

    /* V накапливает собственные векторы; стартуем с единичной матрицы. */
    for (i = 0; i < n * n; ++i) V[i] = 0.0;
    for (i = 0; i < n; ++i) V[i * n + i] = 1.0;

    for (iter = 0; iter < max_iter; ++iter) {
        int p = 0, q = 1;
        double max_off = 0.0;

        /* Ищем самый большой внедиагональный элемент. */
        for (i = 0; i < n; ++i) {
            for (j = i + 1; j < n; ++j) {
                double v = fabs(A[i * n + j]);
                if (v > max_off) {
                    max_off = v;
                    p = i;
                    q = j;
                }
            }
        }
        if (max_off < eps) break;

        {
            double app = A[p * n + p];
            double aqq = A[q * n + q];
            double apq = A[p * n + q];
            double tau = (aqq - app) / (2.0 * apq);
            double t = (tau >= 0.0) ? 1.0 / (tau + sqrt(1.0 + tau * tau))
                                    : -1.0 / (-tau + sqrt(1.0 + tau * tau));
            double c = 1.0 / sqrt(1.0 + t * t);
            double s = t * c;
            double new_app = app - t * apq;
            double new_aqq = aqq + t * apq;

            /* Делаем поворот Якоби и зануляем выбранный элемент. */
            for (j = 0; j < n; ++j) {
                if (j != p && j != q) {
                    double aip = A[j * n + p];
                    double aiq = A[j * n + q];
                    A[j * n + p] = A[p * n + j] = c * aip - s * aiq;
                    A[j * n + q] = A[q * n + j] = s * aip + c * aiq;
                }
            }
            A[p * n + p] = new_app;
            A[q * n + q] = new_aqq;
            A[p * n + q] = A[q * n + p] = 0.0;

            /* Одновременно обновляем матрицу собственных векторов. */
            for (j = 0; j < n; ++j) {
                double vjp = V[j * n + p];
                double vjq = V[j * n + q];
                V[j * n + p] = c * vjp - s * vjq;
                V[j * n + q] = s * vjp + c * vjq;
            }
        }
    }

    for (i = 0; i < n; ++i) eigenvalues[i] = A[i * n + i];
}

static void sort_eigenpairs_desc(double *evals, double *V, int n) {
    int i, j, k;

    /* Сортируем собственные значения по убыванию вместе с их векторами. */
    for (i = 0; i < n - 1; ++i) {
        int best = i;
        for (j = i + 1; j < n; ++j) {
            if (evals[j] > evals[best]) best = j;
        }
        if (best != i) {
            double tmp = evals[i];
            evals[i] = evals[best];
            evals[best] = tmp;
            for (k = 0; k < n; ++k) {
                double t = V[k * n + i];
                V[k * n + i] = V[k * n + best];
                V[k * n + best] = t;
            }
        }
    }
}

static int write_exact(FILE *fp, const void *data, size_t size, size_t count) {
    if (!fp) return 1;
    return fwrite(data, size, count, fp) == count;
}

static int svd_approx_channel(const double *A, int m, int n, int k, double *approx, FILE *fp) {
    double *ATA = (double *)malloc((size_t)n * n * sizeof(double));
    double *ATA_copy = (double *)malloc((size_t)n * n * sizeof(double));
    double *evals = (double *)malloc((size_t)n * sizeof(double));
    double *V = (double *)malloc((size_t)n * n * sizeof(double));
    double *vcol = (double *)malloc((size_t)n * sizeof(double));
    int i, r, c;

    if (!ATA || !ATA_copy || !evals || !V || !vcol) {
        free(ATA);
        free(ATA_copy);
        free(evals);
        free(V);
        free(vcol);
        return 0;
    }

    build_ata(A, m, n, ATA);
    memcpy(ATA_copy, ATA, (size_t)n * n * sizeof(double));
    jacobi_eigen(ATA_copy, n, evals, V);
    sort_eigenpairs_desc(evals, V, n);
    if (k > n) k = n;

    /* Собираем приближение только по первым k компонентам. */
    for (i = 0; i < k; ++i) {
        double lambda = evals[i];
        double *av = (double *)calloc((size_t)m, sizeof(double));
        if (!av) {
            free(ATA);
            free(ATA_copy);
            free(evals);
            free(V);
            free(vcol);
            return 0;
        }

        for (c = 0; c < n; ++c) vcol[c] = 0.0;

        if (lambda > 1e-8) {
            /* av = A * v_i. Это уже sigma_i * u_i, удобно хранить в файле. */
            for (c = 0; c < n; ++c) vcol[c] = V[c * n + i];
            for (r = 0; r < m; ++r) {
                double sum = 0.0;
                for (c = 0; c < n; ++c) sum += A[r * n + c] * vcol[c];
                av[r] = sum;
            }

            /* Добавляем ранговую компоненту в итоговую матрицу. */
            for (r = 0; r < m; ++r) {
                for (c = 0; c < n; ++c) {
                    approx[r * n + c] += av[r] * vcol[c];
                }
            }
        }

        if (!write_exact(fp, av, sizeof(double), (size_t)m) ||
            !write_exact(fp, vcol, sizeof(double), (size_t)n)) {
            free(av);
            free(ATA);
            free(ATA_copy);
            free(evals);
            free(V);
            free(vcol);
            return 0;
        }
        free(av);
    }

    free(ATA);
    free(ATA_copy);
    free(evals);
    free(V);
    free(vcol);
    return 1;
}

static int write_u32_le(FILE *fp, uint32_t v) {
    unsigned char b[4];
    b[0] = (unsigned char)(v & 255);
    b[1] = (unsigned char)((v >> 8) & 255);
    b[2] = (unsigned char)((v >> 16) & 255);
    b[3] = (unsigned char)((v >> 24) & 255);
    return fwrite(b, 1, 4, fp) == 4;
}

static int write_svd_header(FILE *fp, int width, int height, int channels, int k) {
    if (fwrite("SVD1", 1, 4, fp) != 4) return 0;
    if (!write_u32_le(fp, (uint32_t)width)) return 0;
    if (!write_u32_le(fp, (uint32_t)height)) return 0;
    if (!write_u32_le(fp, (uint32_t)channels)) return 0;
    if (!write_u32_le(fp, (uint32_t)k)) return 0;
    return 1;
}

static int alloc_output_like(const Image *img, Image *out) {
    out->width = img->width;
    out->height = img->height;
    out->channels = img->channels;
    out->data = (unsigned char *)malloc((size_t)img->width * img->height * img->channels);
    if (!out->data) {
        out->width = 0;
        out->height = 0;
        out->channels = 0;
        return 0;
    }
    return 1;
}

static int svd_compress_internal(const Image *img, int k, const char *filename, Image *out, RunStats *stats) {
    int m;
    int n;
    int c;
    FILE *fp = NULL;
    clock_t t0 = clock();

    if (!img || !img->data || img->channels != 3 || !out) return 0;
    m = img->height;
    n = img->width;
    if (k < 1) k = 1;
    if (k > n) k = n;

    out->width = 0;
    out->height = 0;
    out->channels = 0;
    out->data = NULL;

    if (!alloc_output_like(img, out)) return 0;

    if (filename) {
        fp = fopen(filename, "wb");
        if (!fp) {
            free_image(out);
            return 0;
        }
        if (!write_svd_header(fp, img->width, img->height, img->channels, k)) {
            fclose(fp);
            free_image(out);
            return 0;
        }
    }

    /* SVD приближение строим отдельно для каждого канала. */
    for (c = 0; c < img->channels; ++c) {
        double *A = image_to_matrix_channel(img, c);
        double *approx = (double *)calloc((size_t)m * n, sizeof(double));
        if (!A || !approx || !svd_approx_channel(A, m, n, k, approx, fp)) {
            free(A);
            free(approx);
            if (fp) fclose(fp);
            free_image(out);
            return 0;
        }
        write_matrix_to_channel(out->data, approx, n, m, img->channels, c);
        free(A);
        free(approx);
    }

    if (fp && fclose(fp) != 0) {
        free_image(out);
        return 0;
    }

    if (stats) {
        stats->seconds = (double)(clock() - t0) / CLOCKS_PER_SEC;
        stats->psnr = psnr_metric(img, out);
        stats->ssim = ssim_metric(img, out);
        {
            /* Реальный объём .svd: заголовок + для каждого канала k пар векторов. */
            double original_bytes = (double)(m * n * img->channels);
            double stored_bytes = 20.0 + (double)img->channels * k * (m + n) * sizeof(double);
            stats->ratio = stored_bytes > 0.0 ? original_bytes / stored_bytes : 0.0;
        }
    }

    return 1;
}

Image svd_compress(const Image *img, int k, RunStats *stats) {
    Image out = {0, 0, 0, NULL};
    svd_compress_internal(img, k, NULL, &out, stats);
    return out;
}

int svd_compress_to_file(const Image *img, int k, const char *filename, Image *out, RunStats *stats) {
    return svd_compress_internal(img, k, filename, out, stats);
}
