// Self-test for the EGL capture pipeline that needs no root and no KMS access.
//
// It creates a linear GBM buffer, paints a known pattern into it (top half red,
// bottom half blue), exports it as a dma-buf and runs it through the exact same
// ds_egl_capture_dmabuf() path used for real screen framebuffers. It then checks
// a few pixels and writes the result to PNG. If this passes, the only untested
// link in the real capture is "KMS handed us the screen's dma-buf" -- which only
// requires running as root.

#include "../include/egl_capture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <drm_fourcc.h>

#define W 256
#define H 256

int main(int argc, char **argv) {
    const char *out = argc > 1 ? argv[1] : "selftest.png";

    ds_egl *e = ds_egl_create(NULL);
    if (!e) {
        fprintf(stderr, "ds_egl_create failed\n");
        return 1;
    }

    struct gbm_bo *bo = gbm_bo_create(e->gbm, W, H, GBM_FORMAT_XRGB8888,
                                      GBM_BO_USE_LINEAR | GBM_BO_USE_RENDERING);
    if (!bo) {
        fprintf(stderr, "gbm_bo_create failed\n");
        ds_egl_destroy(e);
        return 1;
    }

    // Paint the pattern: top half red, bottom half blue (XRGB8888 word 0xXXRRGGBB).
    void *map_data = NULL;
    uint32_t stride = 0;
    void *ptr = gbm_bo_map(bo, 0, 0, W, H, GBM_BO_TRANSFER_WRITE, &stride, &map_data);
    if (!ptr) {
        fprintf(stderr, "gbm_bo_map failed\n");
        gbm_bo_destroy(bo);
        ds_egl_destroy(e);
        return 1;
    }
    for (uint32_t y = 0; y < H; ++y) {
        uint32_t *row = (uint32_t *)((uint8_t *)ptr + (size_t)y * stride);
        uint32_t color = (y < H / 2) ? 0x00FF0000u : 0x000000FFu;
        for (uint32_t x = 0; x < W; ++x)
            row[x] = color;
    }
    gbm_bo_unmap(bo, map_data);

    ds_dmabuf_desc desc = {0};
    desc.fourcc = DRM_FORMAT_XRGB8888;
    desc.width = W;
    desc.height = H;
    desc.modifier = gbm_bo_get_modifier(bo);
    desc.num_planes = 1;
    desc.fds[0] = gbm_bo_get_fd(bo);
    desc.offsets[0] = gbm_bo_get_offset(bo, 0);
    desc.pitches[0] = gbm_bo_get_stride(bo);

    if (desc.fds[0] < 0) {
        fprintf(stderr, "gbm_bo_get_fd failed\n");
        gbm_bo_destroy(bo);
        ds_egl_destroy(e);
        return 1;
    }

    uint8_t *rgba = NULL;
    uint32_t ow = 0, oh = 0;
    int rc = ds_egl_capture_dmabuf(e, &desc, &rgba, &ow, &oh);
    close(desc.fds[0]);

    if (rc != 0) {
        fprintf(stderr, "ds_egl_capture_dmabuf failed: %d\n", rc);
        gbm_bo_destroy(bo);
        ds_egl_destroy(e);
        return 1;
    }

    // Verify orientation and color: row 8 should be red, row H-8 should be blue.
    const uint8_t *top = rgba + (size_t)8 * ow * 4;
    const uint8_t *bot = rgba + (size_t)(oh - 8) * ow * 4;
    int ok = top[0] > 200 && top[1] < 50 && top[2] < 50 &&
             bot[0] < 50 && bot[1] < 50 && bot[2] > 200;
    printf("top  pixel RGBA = %3u %3u %3u %3u (expect red)\n",  top[0], top[1], top[2], top[3]);
    printf("bottom pixel RGBA = %3u %3u %3u %3u (expect blue)\n", bot[0], bot[1], bot[2], bot[3]);

    if (ds_save_png(out, rgba, ow, oh) == 0)
        printf("wrote %s (%ux%u)\n", out, ow, oh);

    free(rgba);
    gbm_bo_destroy(bo);
    ds_egl_destroy(e);

    if (!ok) {
        fprintf(stderr, "SELFTEST FAILED: pixels/orientation wrong\n");
        return 1;
    }
    printf("SELFTEST PASSED\n");
    return 0;
}
