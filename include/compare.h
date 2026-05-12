#ifndef COMPARE_H
#define COMPARE_H

/*
 * Запускает пакетное сравнение для нескольких значений k и quality.
 * Результаты сохраняются в .svd/.jpg и summary.csv.
 */
int run_compare_mode(const char *input_path, const char *out_dir, const char *k_list, const char *quality_list);

#endif
