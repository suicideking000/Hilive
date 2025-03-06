/*
 * Copyright (c) 2017 Liming Shao <lmshao@163.com>
 */

#ifndef HISILIVE_UTILS_H
#define HISILIVE_UTILS_H

#include <stdint.h>

#define LOG(fmt...)                                                                                                                        \
    do {                                                                                                                                   \
        printf("[%s:%d]:", __FUNCTION__, __LINE__);                                                                                        \
        printf(fmt);                                                                                                                       \
    } while (0)

#define LOGD(fmt...)                                                                                                                       \
    do {                                                                                                                                   \
        printf("\033[32m");                                                                                                                \
        printf("[%s:%d]:", __FUNCTION__, __LINE__);                                                                                        \
        printf(fmt);                                                                                                                       \
        printf("\033[0m");                                                                                                                 \
    } while (0)

#define GREEN(fmt...)                                                                                                                      \
    do {                                                                                                                                   \
        printf("\033[32m");                                                                                                                \
        printf(fmt);                                                                                                                       \
        printf("\033[0m");                                                                                                                 \
    } while (0)

#define LOGE(fmt...)                                                                                                                       \
    do {                                                                                                                                   \
        printf("\033[31m");                                                                                                                \
        printf("[%s:%d]:", __FUNCTION__, __LINE__);                                                                                        \
        printf(fmt);                                                                                                                       \
        printf("\033[0m");                                                                                                                 \
    } while (0)

#define RED(fmt...)                                                                                                                        \
    do {                                                                                                                                   \
        printf("\033[31m");                                                                                                                \
        printf(fmt);                                                                                                                       \
        printf("\033[0m");                                                                                                                 \
    } while (0)

uint8_t *Load8(uint8_t *p, uint8_t x);

uint8_t *Load16(uint8_t *p, uint16_t x);

uint8_t *Load32(uint8_t *p, uint32_t x);

int writeFile(char *filename, char *data, int len, int append);

void dumpHex(const uint8_t *ptr, int len);

char *getCurrentTime();

#endif  // HISILIVE_UTILS_H
