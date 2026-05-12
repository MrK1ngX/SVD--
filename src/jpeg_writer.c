#include "jpeg_writer.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define JPG_BLOCK 8

static const unsigned char zigzag[64] = {
    0, 1, 8,16, 9, 2, 3,10,
   17,24,32,25,18,11, 4, 5,
   12,19,26,33,40,48,41,34,
   27,20,13, 6, 7,14,21,28,
   35,42,49,56,57,50,43,36,
   29,22,15,23,30,37,44,51,
   58,59,52,45,38,31,39,46,
   53,60,61,54,47,55,62,63
};

static const unsigned char base_luma_q[64] = {
    16,11,10,16,24,40,51,61,
    12,12,14,19,26,58,60,55,
    14,13,16,24,40,57,69,56,
    14,17,22,29,51,87,80,62,
    18,22,37,56,68,109,103,77,
    24,35,55,64,81,104,113,92,
    49,64,78,87,103,121,120,101,
    72,92,95,98,112,100,103,99
};

static const unsigned char base_chroma_q[64] = {
    17,18,24,47,99,99,99,99,
    18,21,26,66,99,99,99,99,
    24,26,56,99,99,99,99,99,
    47,66,99,99,99,99,99,99,
    99,99,99,99,99,99,99,99,
    99,99,99,99,99,99,99,99,
    99,99,99,99,99,99,99,99,
    99,99,99,99,99,99,99,99
};

static const unsigned char bits_dc_luma[16]   = {0,1,5,1,1,1,1,1,1,0,0,0,0,0,0,0};
static const unsigned char vals_dc_luma[12]   = {0,1,2,3,4,5,6,7,8,9,10,11};
static const unsigned char bits_dc_chroma[16] = {0,3,1,1,1,1,1,1,1,1,1,0,0,0,0,0};
static const unsigned char vals_dc_chroma[12] = {0,1,2,3,4,5,6,7,8,9,10,11};

static const unsigned char bits_ac_luma[16] = {0,2,1,3,3,2,4,3,5,5,4,4,0,0,1,125};
static const unsigned char vals_ac_luma[162] = {
    0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,
    0x21,0x31,0x41,0x06,0x13,0x51,0x61,0x07,
    0x22,0x71,0x14,0x32,0x81,0x91,0xa1,0x08,
    0x23,0x42,0xb1,0xc1,0x15,0x52,0xd1,0xf0,
    0x24,0x33,0x62,0x72,0x82,0x09,0x0a,0x16,
    0x17,0x18,0x19,0x1a,0x25,0x26,0x27,0x28,
    0x29,0x2a,0x34,0x35,0x36,0x37,0x38,0x39,
    0x3a,0x43,0x44,0x45,0x46,0x47,0x48,0x49,
    0x4a,0x53,0x54,0x55,0x56,0x57,0x58,0x59,
    0x5a,0x63,0x64,0x65,0x66,0x67,0x68,0x69,
    0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,
    0x7a,0x83,0x84,0x85,0x86,0x87,0x88,0x89,
    0x8a,0x92,0x93,0x94,0x95,0x96,0x97,0x98,
    0x99,0x9a,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,
    0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,
    0xb7,0xb8,0xb9,0xba,0xc2,0xc3,0xc4,0xc5,
    0xc6,0xc7,0xc8,0xc9,0xca,0xd2,0xd3,0xd4,
    0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xe1,0xe2,
    0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,
    0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,
    0xf9,0xfa
};

static const unsigned char bits_ac_chroma[16] = {0,2,1,2,4,4,3,4,7,5,4,4,0,1,2,119};
static const unsigned char vals_ac_chroma[162] = {
    0x00,0x01,0x02,0x03,0x11,0x04,0x05,0x21,
    0x31,0x06,0x12,0x41,0x51,0x07,0x61,0x71,
    0x13,0x22,0x32,0x81,0x08,0x14,0x42,0x91,
    0xa1,0xb1,0xc1,0x09,0x23,0x33,0x52,0xf0,
    0x15,0x62,0x72,0xd1,0x0a,0x16,0x24,0x34,
    0xe1,0x25,0xf1,0x17,0x18,0x19,0x1a,0x26,
    0x27,0x28,0x29,0x2a,0x35,0x36,0x37,0x38,
    0x39,0x3a,0x43,0x44,0x45,0x46,0x47,0x48,
    0x49,0x4a,0x53,0x54,0x55,0x56,0x57,0x58,
    0x59,0x5a,0x63,0x64,0x65,0x66,0x67,0x68,
    0x69,0x6a,0x73,0x74,0x75,0x76,0x77,0x78,
    0x79,0x7a,0x82,0x83,0x84,0x85,0x86,0x87,
    0x88,0x89,0x8a,0x92,0x93,0x94,0x95,0x96,
    0x97,0x98,0x99,0x9a,0xa2,0xa3,0xa4,0xa5,
    0xa6,0xa7,0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,
    0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xc2,0xc3,
    0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xd2,
    0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,
    0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,
    0xea,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,
    0xf9,0xfa
};

typedef struct {
    unsigned short code;
    unsigned char size;
} HuffCode;

typedef struct {
    FILE *fp;
    unsigned char byte;
    int bits_used;
} BitWriter;

static void write_u16_be(FILE *fp, unsigned int v) {
    fputc((int)((v >> 8) & 255), fp);
    fputc((int)(v & 255), fp);
}

static void write_marker(FILE *fp, unsigned char marker) {
    fputc(0xFF, fp);
    fputc(marker, fp);
}

static int huff_count(const unsigned char bits[16]) {
    int i, total = 0;
    for (i = 0; i < 16; ++i) total += bits[i];
    return total;
}

static void build_huffman(const unsigned char bits[16], const unsigned char *vals, HuffCode table[256]) {
    unsigned int code = 0;
    int i, j, k = 0;

    for (i = 0; i < 256; ++i) {
        table[i].code = 0;
        table[i].size = 0;
    }

    for (i = 1; i <= 16; ++i) {
        for (j = 0; j < bits[i - 1]; ++j) {
            table[vals[k]].code = (unsigned short)code;
            table[vals[k]].size = (unsigned char)i;
            ++code;
            ++k;
        }
        code <<= 1;
    }
}

static void write_dht(FILE *fp, int table_class, int table_id, const unsigned char bits[16], const unsigned char *vals) {
    int count = huff_count(bits);
    int i;

    write_marker(fp, 0xC4);
    write_u16_be(fp, 2 + 1 + 16 + count);
    fputc((table_class << 4) | table_id, fp);
    for (i = 0; i < 16; ++i) fputc(bits[i], fp);
    for (i = 0; i < count; ++i) fputc(vals[i], fp);
}

static void build_qtable(int quality, const unsigned char *base, unsigned char out[64]) {
    int i, scale;

    quality = clamp_int(quality, 1, 100);
    scale = (quality < 50) ? (5000 / quality) : (200 - quality * 2);
    for (i = 0; i < 64; ++i) {
        int q = (base[i] * scale + 50) / 100;
        out[i] = (unsigned char)clamp_int(q, 1, 255);
    }
}

static void write_dqt(FILE *fp, int table_id, const unsigned char qtable[64]) {
    int i;

    write_marker(fp, 0xDB);
    write_u16_be(fp, 67);
    fputc(table_id, fp);
    for (i = 0; i < 64; ++i) fputc(qtable[zigzag[i]], fp);
}

static void write_app0(FILE *fp) {
    write_marker(fp, 0xE0);
    write_u16_be(fp, 16);
    fputc('J', fp); fputc('F', fp); fputc('I', fp); fputc('F', fp); fputc(0, fp);
    fputc(1, fp); fputc(1, fp);
    fputc(0, fp);
    write_u16_be(fp, 1);
    write_u16_be(fp, 1);
    fputc(0, fp); fputc(0, fp);
}

static void write_sof0(FILE *fp, int width, int height) {
    write_marker(fp, 0xC0);
    write_u16_be(fp, 17);
    fputc(8, fp);
    write_u16_be(fp, (unsigned int)height);
    write_u16_be(fp, (unsigned int)width);
    fputc(3, fp);
    fputc(1, fp); fputc(0x11, fp); fputc(0, fp);
    fputc(2, fp); fputc(0x11, fp); fputc(1, fp);
    fputc(3, fp); fputc(0x11, fp); fputc(1, fp);
}

static void write_sos(FILE *fp) {
    write_marker(fp, 0xDA);
    write_u16_be(fp, 12);
    fputc(3, fp);
    fputc(1, fp); fputc(0x00, fp);
    fputc(2, fp); fputc(0x11, fp);
    fputc(3, fp); fputc(0x11, fp);
    fputc(0, fp);
    fputc(63, fp);
    fputc(0, fp);
}

static void bw_init(BitWriter *bw, FILE *fp) {
    bw->fp = fp;
    bw->byte = 0;
    bw->bits_used = 0;
}

static void bw_put_byte(BitWriter *bw, unsigned char v) {
    fputc(v, bw->fp);
    if (v == 0xFF) fputc(0, bw->fp);
}

static void bw_write_bits(BitWriter *bw, unsigned int code, int size) {
    int i;
    for (i = size - 1; i >= 0; --i) {
        bw->byte = (unsigned char)((bw->byte << 1) | ((code >> i) & 1));
        bw->bits_used++;
        if (bw->bits_used == 8) {
            bw_put_byte(bw, bw->byte);
            bw->byte = 0;
            bw->bits_used = 0;
        }
    }
}

static void bw_write_huff(BitWriter *bw, const HuffCode table[256], int symbol) {
    bw_write_bits(bw, table[symbol].code, table[symbol].size);
}

static void bw_flush(BitWriter *bw) {
    if (bw->bits_used > 0) {
        bw->byte = (unsigned char)(bw->byte << (8 - bw->bits_used));
        bw->byte |= (unsigned char)((1 << (8 - bw->bits_used)) - 1);
        bw_put_byte(bw, bw->byte);
        bw->byte = 0;
        bw->bits_used = 0;
    }
}

static int value_category(int value) {
    int abs_value = value < 0 ? -value : value;
    int category = 0;
    while (abs_value) {
        category++;
        abs_value >>= 1;
    }
    return category;
}

static unsigned int value_bits(int value, int category) {
    if (category == 0) return 0;
    if (value >= 0) return (unsigned int)value;
    return (unsigned int)(value + ((1 << category) - 1));
}

static void dct8(const double in[64], double out[64]) {
    int u, v, x, y;

    for (u = 0; u < 8; ++u) {
        for (v = 0; v < 8; ++v) {
            double cu = (u == 0) ? 1.0 / sqrt(2.0) : 1.0;
            double cv = (v == 0) ? 1.0 / sqrt(2.0) : 1.0;
            double sum = 0.0;
            for (x = 0; x < 8; ++x) {
                for (y = 0; y < 8; ++y) {
                    sum += in[x * 8 + y] *
                           cos(((2 * x + 1) * u * M_PI) / 16.0) *
                           cos(((2 * y + 1) * v * M_PI) / 16.0);
                }
            }
            out[u * 8 + v] = 0.25 * cu * cv * sum;
        }
    }
}

static void make_component_block(const Image *img, int bx, int by, int component, double block[64]) {
    int x, y;

    for (y = 0; y < 8; ++y) {
        for (x = 0; x < 8; ++x) {
            int sx = bx + x;
            int sy = by + y;
            size_t idx;
            double r, g, b, v;

            if (sx >= img->width) sx = img->width - 1;
            if (sy >= img->height) sy = img->height - 1;
            idx = ((size_t)sy * img->width + sx) * img->channels;
            r = img->data[idx + 0];
            g = img->data[idx + 1];
            b = img->data[idx + 2];

            if (component == 0) {
                v = 0.299 * r + 0.587 * g + 0.114 * b;
            } else if (component == 1) {
                v = -0.168736 * r - 0.331264 * g + 0.5 * b + 128.0;
            } else {
                v = 0.5 * r - 0.418688 * g - 0.081312 * b + 128.0;
            }
            block[y * 8 + x] = v - 128.0;
        }
    }
}

static void encode_block(BitWriter *bw, const Image *img, int bx, int by, int component,
                         const unsigned char qtable[64], int *prev_dc,
                         const HuffCode dc_table[256], const HuffCode ac_table[256]) {
    double block[64], coeff[64];
    int qcoeff[64];
    int zz[64];
    int i, run;
    int dc_diff, cat;

    make_component_block(img, bx, by, component, block);
    dct8(block, coeff);

    for (i = 0; i < 64; ++i) {
        qcoeff[i] = (int)llround(coeff[i] / qtable[i]);
    }
    for (i = 0; i < 64; ++i) zz[i] = qcoeff[zigzag[i]];

    dc_diff = zz[0] - *prev_dc;
    *prev_dc = zz[0];
    cat = value_category(dc_diff);
    bw_write_huff(bw, dc_table, cat);
    if (cat > 0) bw_write_bits(bw, value_bits(dc_diff, cat), cat);

    run = 0;
    for (i = 1; i < 64; ++i) {
        if (zz[i] == 0) {
            run++;
        } else {
            while (run > 15) {
                bw_write_huff(bw, ac_table, 0xF0);
                run -= 16;
            }
            cat = value_category(zz[i]);
            bw_write_huff(bw, ac_table, (run << 4) | cat);
            bw_write_bits(bw, value_bits(zz[i], cat), cat);
            run = 0;
        }
    }
    if (run > 0) bw_write_huff(bw, ac_table, 0x00);
}

int write_jpeg(const char *filename, const Image *img, int quality) {
    FILE *fp;
    unsigned char q_luma[64], q_chroma[64];
    HuffCode dc_luma[256], ac_luma[256], dc_chroma[256], ac_chroma[256];
    BitWriter bw;
    int prev_dc[3] = {0, 0, 0};
    int bx, by;

    if (!filename || !img || !img->data || img->channels != 3 || img->width <= 0 || img->height <= 0) return 0;
    if (img->width > 65535 || img->height > 65535) return 0;

    fp = fopen(filename, "wb");
    if (!fp) return 0;

    build_qtable(quality, base_luma_q, q_luma);
    build_qtable(quality, base_chroma_q, q_chroma);
    build_huffman(bits_dc_luma, vals_dc_luma, dc_luma);
    build_huffman(bits_ac_luma, vals_ac_luma, ac_luma);
    build_huffman(bits_dc_chroma, vals_dc_chroma, dc_chroma);
    build_huffman(bits_ac_chroma, vals_ac_chroma, ac_chroma);

    write_marker(fp, 0xD8);
    write_app0(fp);
    write_dqt(fp, 0, q_luma);
    write_dqt(fp, 1, q_chroma);
    write_sof0(fp, img->width, img->height);
    write_dht(fp, 0, 0, bits_dc_luma, vals_dc_luma);
    write_dht(fp, 1, 0, bits_ac_luma, vals_ac_luma);
    write_dht(fp, 0, 1, bits_dc_chroma, vals_dc_chroma);
    write_dht(fp, 1, 1, bits_ac_chroma, vals_ac_chroma);
    write_sos(fp);

    bw_init(&bw, fp);
    for (by = 0; by < img->height; by += 8) {
        for (bx = 0; bx < img->width; bx += 8) {
            encode_block(&bw, img, bx, by, 0, q_luma, &prev_dc[0], dc_luma, ac_luma);
            encode_block(&bw, img, bx, by, 1, q_chroma, &prev_dc[1], dc_chroma, ac_chroma);
            encode_block(&bw, img, bx, by, 2, q_chroma, &prev_dc[2], dc_chroma, ac_chroma);
        }
    }
    bw_flush(&bw);
    write_marker(fp, 0xD9);

    if (fclose(fp) != 0) return 0;
    return 1;
}
