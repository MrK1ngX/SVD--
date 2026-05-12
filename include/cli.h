#ifndef CLI_H
#define CLI_H

#include "common.h"

/* Печатает метрики после выполнения алгоритма. */
void print_stats(const char *name, RunStats stats);

/* Показывает справку по запуску программы. */
void usage(const char *prog);

#endif
