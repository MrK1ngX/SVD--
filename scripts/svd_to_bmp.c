/*
 * Restores a 24-bit BMP image from this project's binary .svd file.
 *
 * Build:
 *   gcc scripts/svd_to_bmp.c -Wall -Wextra -Wpedantic -std=c11 -O2 -o svd_to_bmp
 *
 * Usage:
 *   svd_to_bmp input.svd output.bmp
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int read_exact(FILE *fp, void *data, size_t size) {
    return fread(data, 1, size, fp) == size;
}

static int write_exact(FILE *fp, const void *data, size_t size) {
    return fwrite(data, 1, size, fp) == size;
}

static uint32_t read_u32_le_from_bytes(const unsigned char b[4]) {
    return ((uint32_t)b[0]) |
           ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) |
           ((uint32_t)b[3] << 24);
}

static int read_u32_le(FILE *fp, uint32_t *out) {
    unsigned char b[4];
    if (!read_exact(fp, b, sizeof(b))) return 0;
    *out = read_u32_le_from_bytes(b);
    return 1;
}

static int write_u16_le(FILE *fp, uint16_t v) {
    unsigned char b[2];
    b[0] = (unsigned char)(v & 255u);
    b[1] = (unsigned char)((v >> 8) & 255u);
    return write_exact(fp, b, sizeof(b));
}

static int write_u32_le(FILE *fp, uint32_t v) {
    unsigned char b[4];
    b[0] = (unsigned char)(v & 255u);
    b[1] = (unsigned char)((v >> 8) & 255u);
    b[2] = (unsigned char)((v >> 16) & 255u);
    b[3] = (unsigned char)((v >> 24) & 255u);
    return write_exact(fp, b, sizeof(b));
}

static int write_i32_le(FILE *fp, int32_t v) {
    return write_u32_le(fp, (uint32_t)v);
}

static unsigned char clamp_byte(double value) {
    long rounded = lround(value);
    if (rounded < 0) return 0;
    if (rounded > 255) return 255;
    return (unsigned char)rounded;
}

static int checked_mul_size(size_t a, size_t b, size_t *out) {
    if (a != 0 && b > ((size_t)-1) / a) return 0;
    *out = a * b;
    return 1;
}

static int load_svd(const char *path, uint32_t *width, uint32_t *height, unsigned char **pixels) {
    FILE *fp = fopen(path, "rb");
    unsigned char magic[4];
    uint32_t channels;
    uint32_t k;
    size_t pixel_count;
    size_t byte_count;
    unsigned char *rgb = NULL;
    uint32_t channel;

    if (!fp) {
        fprintf(stderr, "Cannot open input file: %s\n", path);
        return 0;
    }

    if (!read_exact(fp, magic, sizeof(magic)) || memcmp(magic, "SVD1", 4) != 0 ||
        !read_u32_le(fp, width) || !read_u32_le(fp, height) ||
        !read_u32_le(fp, &channels) || !read_u32_le(fp, &k)) {
        fprintf(stderr, "Invalid or truncated .svd header.\n");
        fclose(fp);
        return 0;
    }

    if (*width == 0 || *height == 0 || channels != 3 || k == 0) {
        fprintf(stderr, "Unsupported .svd header: width=%u height=%u channels=%u k=%u\n",
                *width, *height, channels, k);
        fclose(fp);
        return 0;
    }

    if (!checked_mul_size((size_t)(*width), (size_t)(*height), &pixel_count) ||
        !checked_mul_size(pixel_count, (size_t)channels, &byte_count)) {
        fprintf(stderr, "Image dimensions are too large.\n");
        fclose(fp);
        return 0;
    }

    rgb = (unsigned char *)calloc(byte_count, 1);
    if (!rgb) {
        fprintf(stderr, "Not enough memory for output image.\n");
        fclose(fp);
        return 0;
    }

    for (channel = 0; channel < channels; ++channel) {
        double *acc = (double *)calloc(pixel_count, sizeof(double));
        double *left = (double *)malloc((size_t)(*height) * sizeof(double));
        double *right = (double *)malloc((size_t)(*width) * sizeof(double));
        uint32_t component;

        if (!acc || !left || !right) {
            fprintf(stderr, "Not enough memory while restoring channel.\n");
            free(acc);
            free(left);
            free(right);
            free(rgb);
            fclose(fp);
            return 0;
        }

        for (component = 0; component < k; ++component) {
            uint32_t y;

            if (!read_exact(fp, left, (size_t)(*height) * sizeof(double)) ||
                !read_exact(fp, right, (size_t)(*width) * sizeof(double))) {
                fprintf(stderr, "Unexpected end of .svd payload.\n");
                free(acc);
                free(left);
                free(right);
                free(rgb);
                fclose(fp);
                return 0;
            }

            for (y = 0; y < *height; ++y) {
                uint32_t x;
                size_t row = (size_t)y * (*width);
                double lv = left[y];
                for (x = 0; x < *width; ++x) {
                    acc[row + x] += lv * right[x];
                }
            }
        }

        {
            uint32_t y;
            for (y = 0; y < *height; ++y) {
                uint32_t x;
                for (x = 0; x < *width; ++x) {
                    size_t mono_index = (size_t)y * (*width) + x;
                    size_t rgb_index = mono_index * channels + channel;
                    rgb[rgb_index] = clamp_byte(acc[mono_index]);
                }
            }
        }

        free(acc);
        free(left);
        free(right);
    }

    {
        int extra = fgetc(fp);
        if (extra != EOF) {
            fprintf(stderr, "Extra bytes found after expected .svd payload.\n");
            free(rgb);
            fclose(fp);
            return 0;
        }
    }

    fclose(fp);
    *pixels = rgb;
    return 1;
}

static int write_bmp(const char *path, uint32_t width, uint32_t height, const unsigned char *rgb) {
    FILE *fp;
    uint32_t row_size;
    uint32_t padding_size;
    uint32_t image_size;
    uint32_t file_size;
    unsigned char padding[3] = {0, 0, 0};
    int y;

    if (width > 0x1FFFFFFFu || height > 0x1FFFFFFFu) {
        fprintf(stderr, "Image is too large for this BMP writer.\n");
        return 0;
    }

    row_size = ((width * 3u + 3u) / 4u) * 4u;
    padding_size = row_size - width * 3u;
    image_size = row_size * height;
    file_size = 14u + 40u + image_size;

    fp = fopen(path, "wb");
    if (!fp) {
        fprintf(stderr, "Cannot open output file: %s\n", path);
        return 0;
    }

    if (!write_exact(fp, "BM", 2) ||
        !write_u32_le(fp, file_size) ||
        !write_u16_le(fp, 0) ||
        !write_u16_le(fp, 0) ||
        !write_u32_le(fp, 14u + 40u) ||
        !write_u32_le(fp, 40u) ||
        !write_i32_le(fp, (int32_t)width) ||
        !write_i32_le(fp, (int32_t)height) ||
        !write_u16_le(fp, 1) ||
        !write_u16_le(fp, 24) ||
        !write_u32_le(fp, 0) ||
        !write_u32_le(fp, image_size) ||
        !write_i32_le(fp, 2835) ||
        !write_i32_le(fp, 2835) ||
        !write_u32_le(fp, 0) ||
        !write_u32_le(fp, 0)) {
        fprintf(stderr, "Cannot write BMP header.\n");
        fclose(fp);
        return 0;
    }

    for (y = (int)height - 1; y >= 0; --y) {
        uint32_t x;
        for (x = 0; x < width; ++x) {
            size_t idx = ((size_t)y * width + x) * 3u;
            unsigned char bgr[3];
            bgr[0] = rgb[idx + 2];
            bgr[1] = rgb[idx + 1];
            bgr[2] = rgb[idx + 0];
            if (!write_exact(fp, bgr, sizeof(bgr))) {
                fprintf(stderr, "Cannot write BMP pixels.\n");
                fclose(fp);
                return 0;
            }
        }
        if (padding_size > 0 && !write_exact(fp, padding, padding_size)) {
            fprintf(stderr, "Cannot write BMP padding.\n");
            fclose(fp);
            return 0;
        }
    }

    if (fclose(fp) != 0) {
        fprintf(stderr, "Cannot finish BMP file.\n");
        return 0;
    }
    return 1;
}

static void usage(const char *prog) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s <input.svd> <output.bmp>\n", prog);
}

int main(int argc, char **argv) {
    uint32_t width = 0;
    uint32_t height = 0;
    unsigned char *pixels = NULL;

    if (argc != 3) {
        usage(argv[0]);
        return 1;
    }

    if (!load_svd(argv[1], &width, &height, &pixels)) {
        free(pixels);
        return 1;
    }

    if (!write_bmp(argv[2], width, height, pixels)) {
        free(pixels);
        return 1;
    }

    printf("Saved %s (%ux%u)\n", argv[2], width, height);
    free(pixels);
    return 0;
}
