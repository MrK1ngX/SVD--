#include "compare.h"

#include "bmp.h"
#include "common.h"
#include "jpeg_like.h"
#include "jpeg_writer.h"
#include "svd.h"

#include <stdio.h>

int run_compare_mode(const char *input_path, const char *out_dir, const char *k_list, const char *quality_list) {
    Image input = {0, 0, 0, NULL};
    IntList ks, qs;
    FILE *csv;
    char csv_path[512];
    int i;

    /* Читаем списки параметров из командной строки. */
    if (!parse_int_list(k_list, &ks) || !parse_int_list(quality_list, &qs)) {
        fprintf(stderr, "Failed to parse list parameters.\n");
        return 1;
    }

    /* Загружаем исходное изображение один раз. */
    if (!read_bmp(input_path, &input)) {
        fprintf(stderr, "Cannot read BMP: %s\n", input_path);
        return 1;
    }

    /* Готовим папку, куда сложим результаты и CSV-отчёт. */
    if (!ensure_dir(out_dir)) {
        fprintf(stderr, "Cannot create output directory: %s\n", out_dir);
        free_image(&input);
        return 1;
    }

    snprintf(csv_path, sizeof(csv_path), "%s/summary.csv", out_dir);
    csv = fopen(csv_path, "w");
    if (!csv) {
        fprintf(stderr, "Cannot create summary CSV.\n");
        free_image(&input);
        return 1;
    }
    fprintf(csv, "method,parameter,psnr,ssim,ratio,time_seconds\n");
    fclose(csv);

    /* Прогоняем SVD для каждого k. */
    for (i = 0; i < ks.count; ++i) {
        char out_path[512];
        Image out = {0, 0, 0, NULL};
        RunStats stats;

        snprintf(out_path, sizeof(out_path), "%s/svd_k%d.svd", out_dir, ks.values[i]);
        if (!svd_compress_to_file(&input, ks.values[i], out_path, &out, &stats)) {
            fprintf(stderr, "Failed to save %s\n", out_path);
            free_image(&out);
            continue;
        }

        {
            long long in_size = file_size_bytes(input_path);
            long long out_size = file_size_bytes(out_path);
            if (in_size > 0 && out_size > 0) stats.ratio = (double)in_size / (double)out_size;
        }

        /* После каждого запуска дописываем строку в CSV. */
        csv = fopen(csv_path, "a");
        if (csv) {
            fprintf(csv, "svd,%d,%.6f,%.6f,%.6f,%.6f\n", ks.values[i], stats.psnr, stats.ssim, stats.ratio, stats.seconds);
            fclose(csv);
        }
        free_image(&out);
    }

    /* Прогоняем JPEG-подобное сжатие для каждого quality. */
    for (i = 0; i < qs.count; ++i) {
        char out_path[512];
        Image out;
        RunStats stats;

        snprintf(out_path, sizeof(out_path), "%s/jpeg_q%d.jpg", out_dir, qs.values[i]);
        out = jpeg_like_compress(&input, qs.values[i], &stats);
        if (!out.data || !write_jpeg(out_path, &out, qs.values[i])) {
            fprintf(stderr, "Failed to save %s\n", out_path);
            free_image(&out);
            continue;
        }

        {
            long long in_size = file_size_bytes(input_path);
            long long out_size = file_size_bytes(out_path);
            if (in_size > 0 && out_size > 0) stats.ratio = (double)in_size / (double)out_size;
        }

        csv = fopen(csv_path, "a");
        if (csv) {
            fprintf(csv, "jpeg,%d,%.6f,%.6f,%.6f,%.6f\n", qs.values[i], stats.psnr, stats.ssim, stats.ratio, stats.seconds);
            fclose(csv);
        }
        free_image(&out);
    }

    printf("Done. Results saved to %s\n", out_dir);
    free_image(&input);
    return 0;
}
