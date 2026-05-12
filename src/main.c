// .\project\image_compressor.exe svd .\project\app.bmp .\project\out_svd.svd 20
// .\project\image_compressor.exe jpeg .\project\app.bmp .\project\out_jpeg.jpg 60
// .\project\svd_to_bmp.exe .\project\out_svd.svd .\project\out_svd.bmp
#include "bmp.h"
#include "cli.h"
#include "common.h"
#include "compare.h"
#include "jpeg_like.h"
#include "jpeg_writer.h"
#include "svd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *jpeg_output_path(const char *path) {
    if (has_extension_ci(path, ".jpg") || has_extension_ci(path, ".jpeg")) return dup_cstr(path);
    return replace_extension(path, ".jpg");
}

static char *svd_output_path(const char *path) {
    if (has_extension_ci(path, ".svd")) return dup_cstr(path);
    return replace_extension(path, ".svd");
}

int main(int argc, char **argv) {
    Image input = {0, 0, 0, NULL};

    /* Если команда не передана, просто показываем справку. */
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    /* Режим SVD: сжимаем в бинарник .svd, а не в несжатый BMP. */
    if (strcmp(argv[1], "svd") == 0) {
        Image out = {0, 0, 0, NULL};
        RunStats stats;
        int k;
        char *out_path;

        if (argc != 5) {
            usage(argv[0]);
            return 1;
        }
        k = atoi(argv[4]);
        out_path = svd_output_path(argv[3]);
        if (!out_path) {
            fprintf(stderr, "Cannot prepare output path.\n");
            return 1;
        }
        if (strcmp(out_path, argv[3]) != 0) {
            printf("SVD output extension changed to: %s\n", out_path);
        }
        if (!read_bmp(argv[2], &input)) {
            fprintf(stderr, "Cannot read BMP: %s\n", argv[2]);
            free(out_path);
            return 1;
        }
        if (!svd_compress_to_file(&input, k, out_path, &out, &stats)) {
            fprintf(stderr, "Compression failed or cannot write SVD: %s\n", out_path);
            free_image(&input);
            free_image(&out);
            free(out_path);
            return 1;
        }
        {
            long long in_size = file_size_bytes(argv[2]);
            long long out_size = file_size_bytes(out_path);
            if (in_size > 0 && out_size > 0) stats.ratio = (double)in_size / (double)out_size;
        }
        print_stats("SVD result", stats);
        printf("  Saved: %s\n", out_path);
        printf("  Restore BMP: svd_to_bmp.exe %s restored.bmp\n", out_path);
        free_image(&input);
        free_image(&out);
        free(out_path);
        return 0;
    }

    /* Режим JPEG-подобного сжатия сохраняет результат как настоящий .jpg. */
    if (strcmp(argv[1], "jpeg") == 0) {
        Image out;
        RunStats stats;
        int quality;
        char *out_path;

        if (argc != 5) {
            usage(argv[0]);
            return 1;
        }
        quality = atoi(argv[4]);
        out_path = jpeg_output_path(argv[3]);
        if (!out_path) {
            fprintf(stderr, "Cannot prepare output path.\n");
            return 1;
        }
        if (strcmp(out_path, argv[3]) != 0) {
            printf("JPEG output extension changed to: %s\n", out_path);
        }
        if (!read_bmp(argv[2], &input)) {
            fprintf(stderr, "Cannot read BMP: %s\n", argv[2]);
            free(out_path);
            return 1;
        }
        out = jpeg_like_compress(&input, quality, &stats);
        if (!out.data) {
            fprintf(stderr, "Compression failed.\n");
            free_image(&input);
            free(out_path);
            return 1;
        }
        if (!write_jpeg(out_path, &out, quality)) {
            fprintf(stderr, "Cannot write JPEG: %s\n", out_path);
            free_image(&input);
            free_image(&out);
            free(out_path);
            return 1;
        }
        {
            long long in_size = file_size_bytes(argv[2]);
            long long out_size = file_size_bytes(out_path);
            if (in_size > 0 && out_size > 0) stats.ratio = (double)in_size / (double)out_size;
        }
        print_stats("JPEG-like result", stats);
        printf("  Saved: %s\n", out_path);
        free_image(&input);
        free_image(&out);
        free(out_path);
        return 0;
    }

    /* Пакетный режим прогоняет сразу много значений и собирает CSV. */
    if (strcmp(argv[1], "compare") == 0) {
        if (argc != 6) {
            usage(argv[0]);
            return 1;
        }
        return run_compare_mode(argv[2], argv[3], argv[4], argv[5]);
    }

    usage(argv[0]);
    return 1;
}
