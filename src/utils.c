#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>


void setup_dma_buf_attrs(intptr_t *attrs, uint32_t format, uint32_t width, uint32_t height, const int *fds, const uint32_t *offsets, const uint32_t *pitches, const uint32_t *modifies, int num_planes, bool use_modifier) {
   size_t img_attr_index = 0;
   
    attrs[img_attr_index++] = EGL_LINUX_DRM_FOURCC_EXT;
    attrs[img_attr_index++] = format;

    attrs[img_attr_index++] = EGL_WIDTH;
    attrs[img_attr_index++] = width;

    attrs[img_attr_index++] = EGL_HEIGHT;
    attrs[img_attr_index++] = height;

    if (num_planes >= 1) {
        attrs[img_attr_index++] = EGL_DMA_BUF_PLANE0_FD_EXT;
        attrs[img_attr_index++] = fds[0];

        attrs[img_attr_index++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
        attrs[img_attr_index++] = offsets[0];

        attrs[img_attr_index++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
        attrs[img_attr_index++] = pitches[0];

        if (use_modifier) {
            attrs[img_attr_index++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
            attrs[img_attr_index++] = modifies[0] & 0xFFFFFFFFULL; 

            attrs[img_attr_index++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
            attrs[img_attr_index++] = modifies[0] >> 32ULL;
        }
    }
    if (num_planes >= 2) {
        attrs[img_attr_index++] = EGL_DMA_BUF_PLANE0_FD_EXT;
            attrs[img_attr_index++] = fds[1];

            attrs[img_attr_index++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
            attrs[img_attr_index++] = offsets[1];

            attrs[img_attr_index++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
            attrs[img_attr_index++] = pitches[1];

            if (use_modifier) {
                attrs[img_attr_index++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
                attrs[img_attr_index++] = modifies[1] & 0xFFFFFFFFULL; 

                attrs[img_attr_index++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
                attrs[img_attr_index++] = modifies[1] >> 32ULL;
        }
    }
    if (num_planes >= 3) {
        attrs[img_attr_index++] = EGL_DMA_BUF_PLANE0_FD_EXT;
        attrs[img_attr_index++] = fds[2];

        attrs[img_attr_index++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
        attrs[img_attr_index++] = offsets[2];

        attrs[img_attr_index++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
        attrs[img_attr_index++] = pitches[2];

        if (use_modifier) {
            attrs[img_attr_index++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
            attrs[img_attr_index++] = modifies[2] & 0xFFFFFFFFULL; 

            attrs[img_attr_index++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
            attrs[img_attr_index++] = modifies[2] >> 32ULL;
        }
    }
}