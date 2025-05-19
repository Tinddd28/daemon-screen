#ifndef UTILS_H
#define UTILS_H
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

void setup_dma_buf_attrs(intptr_t *attrs, uint32_t format, uint32_t width, uint32_t height, const int *fds, const uint32_t *offsets, const uint32_t *pitches, const uint32_t *modifies, int num_planes, bool use_modifier);

#endif /* UTILS_H */