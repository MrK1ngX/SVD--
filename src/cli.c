#include "cli.h"

#include <stdio.h>

void print_stats(const char *name, RunStats stats) {
    /* Печатаем сводку в одном и том же формате для всех режимов. */
    printf("%s\n", name);
    printf("  PSNR : %.4f dB\n", stats.psnr);
    printf("  SSIM : %.6f\n", stats.ssim);
    printf("  Ratio: %.4f\n", stats.ratio);
    printf("  Time : %.6f s\n", stats.seconds);
}

void usage(const char *prog) {
    /* Короткая подсказка по доступным командам. */
    printf("Usage:\n");
    printf("  %s svd <input.bmp> <output.svd> <k>\n", prog);
    printf("  %s jpeg <input.bmp> <output.jpg> <quality>\n", prog);
    printf("  %s compare <input.bmp> <out_dir> <k_list> <quality_list>\n", prog);
    printf("Examples:\n");
    printf("  %s svd examples/test.bmp out_svd.svd 20\n", prog);
    printf("  %s jpeg examples/test.bmp out_jpeg.jpg 60\n", prog);
    printf("  %s compare examples/test.bmp examples/results 5,10,20,40 20,40,60,80\n", prog);
}
