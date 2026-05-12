#include "common.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

void free_image(Image *img) {
    /* Освобождаем буфер, если он был выделен. */
    if (img && img->data) {
        free(img->data);
        img->data = NULL;
    }

    /* Обнуляем поля, чтобы не осталось старого состояния. */
    if (img) {
        img->width = 0;
        img->height = 0;
        img->channels = 0;
    }
}

int clamp_int(int x, int lo, int hi) {
    /* Обрезаем значение до допустимых границ. */
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

int ensure_dir(const char *path) {
    /* Пытаемся создать папку. Если она уже есть, это не ошибка. */
#ifdef _WIN32
    if (_mkdir(path) == 0) return 1;
#else
    if (mkdir(path, 0777) == 0) return 1;
#endif
    if (errno == EEXIST) return 1;
    return 0;
}

char *dup_cstr(const char *s) {
    /* Делаем обычную копию строки с собственным буфером. */
    size_t n = strlen(s) + 1;
    char *out = (char *)malloc(n);
    if (out) memcpy(out, s, n);
    return out;
}

int parse_int_list(const char *s, IntList *list) {
    char *copy = dup_cstr(s);
    char *tok;

    if (!copy || !list) {
        free(copy);
        return 0;
    }

    /* Разбиваем строку вида "5,10,20" на отдельные числа. */
    list->count = 0;
    tok = strtok(copy, ",");
    while (tok && list->count < 128) {
        list->values[list->count++] = atoi(tok);
        tok = strtok(NULL, ",");
    }

    free(copy);
    return list->count > 0;
}


int has_extension_ci(const char *path, const char *ext) {
    const char *slash1;
    const char *slash2;
    const char *base;
    const char *dot;
    size_t i;

    if (!path || !ext || ext[0] != '.') return 0;
    slash1 = strrchr(path, '/');
    slash2 = strrchr(path, '\\');
    base = path;
    if (slash1 && slash1 + 1 > base) base = slash1 + 1;
    if (slash2 && slash2 + 1 > base) base = slash2 + 1;
    dot = strrchr(base, '.');
    if (!dot) return 0;

    for (i = 0; dot[i] || ext[i]; ++i) {
        unsigned char a = (unsigned char)dot[i];
        unsigned char b = (unsigned char)ext[i];
        if (tolower(a) != tolower(b)) return 0;
    }
    return 1;
}

char *replace_extension(const char *path, const char *ext) {
    const char *slash1;
    const char *slash2;
    const char *base;
    const char *dot;
    size_t prefix_len;
    size_t ext_len;
    char *out;

    if (!path || !ext || ext[0] != '.') return NULL;

    slash1 = strrchr(path, '/');
    slash2 = strrchr(path, '\\');
    base = path;
    if (slash1 && slash1 + 1 > base) base = slash1 + 1;
    if (slash2 && slash2 + 1 > base) base = slash2 + 1;
    dot = strrchr(base, '.');

    prefix_len = dot ? (size_t)(dot - path) : strlen(path);
    ext_len = strlen(ext);
    out = (char *)malloc(prefix_len + ext_len + 1);
    if (!out) return NULL;

    memcpy(out, path, prefix_len);
    memcpy(out + prefix_len, ext, ext_len + 1);
    return out;
}


long long file_size_bytes(const char *path) {
    FILE *fp;
    long size;

    if (!path) return -1;
    fp = fopen(path, "rb");
    if (!fp) return -1;
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    size = ftell(fp);
    fclose(fp);
    if (size < 0) return -1;
    return (long long)size;
}
