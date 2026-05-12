#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>

/* На некоторых платформах M_PI не объявлен по умолчанию. */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Простое представление изображения в памяти. */
typedef struct {
    int width;
    int height;
    int channels;
    unsigned char *data;
} Image;

/* Общие метрики, которые возвращают режимы сжатия. */
typedef struct {
    double psnr;
    double ssim;
    double ratio;
    double seconds;
} RunStats;

/* Список целых чисел для параметров вида 5,10,20,40. */
typedef struct {
    int count;
    int values[128];
} IntList;

/* Освобождает память изображения и обнуляет поля. */
void free_image(Image *img);

/* Ограничивает число диапазоном [lo, hi]. */
int clamp_int(int x, int lo, int hi);

/* Создаёт папку, если её ещё нет. */
int ensure_dir(const char *path);

/* Делает копию C-строки через malloc. */
char *dup_cstr(const char *s);

/* Разбирает строку с числами через запятую в IntList. */
int parse_int_list(const char *s, IntList *list);

/* Проверяет расширение файла без учёта регистра, например .jpg. */
int has_extension_ci(const char *path, const char *ext);

/* Возвращает новый путь с нужным расширением. Результат нужно free(). */
char *replace_extension(const char *path, const char *ext);

/* Возвращает размер файла в байтах или -1 при ошибке. */
long long file_size_bytes(const char *path);


#endif
