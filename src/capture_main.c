// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Shibakov Nikita <tind.nik.28@gmail.com>

// PoC screen capture: enumerate KMS planes, export their framebuffers as
// dma-bufs and read them back as PNG via the EGL pipeline.
//
// Must run as root (or with CAP_SYS_ADMIN): the kernel zeroes the GEM handles
// in drmModeGetFB2 for callers that are neither DRM master nor privileged, so
// an unprivileged process cannot read the compositor's framebuffers. As the
// intended DLP systemd service runs as root, this is the natural fit.

#include "../include/kms.h"
#include "../include/egl_capture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>

// Try each /dev/dri/card* until one yields at least one plane. Returns 0 and
// fills drm/result on success; caller owns the dma-buf fds in result.
static int find_and_capture(ds_drm *drm, ds_kms_result *result, char *card_out, size_t card_out_sz) {
    DIR *d = opendir("/dev/dri");
    if (!d) {
        fprintf(stderr, "cannot open /dev/dri: %s\n", strerror(errno));
        return -1;
    }

    struct dirent *ent;
    int found = -1;
    while ((ent = readdir(d)) != NULL) {
        if (strncmp(ent->d_name, "card", 4) != 0)
            continue;

        char path[300];
        snprintf(path, sizeof(path), "/dev/dri/%s", ent->d_name);

        ds_drm try_drm;
        try_drm.drm_fd = -1;
        if (open_drm_device(path, &try_drm) != 0)
            continue;

        ds_kms_result try_res;
        memset(&try_res, 0, sizeof(try_res));
        kms_get_fb(&try_drm, &try_res);

        if (try_res.num_items > 0) {
            *drm = try_drm;
            *result = try_res;
            snprintf(card_out, card_out_sz, "%s", path);
            found = 0;
            break;
        }

        if (try_res.err_msg[0])
            fprintf(stderr, "%s: %s\n", path, try_res.err_msg);
        close(try_drm.drm_fd);
    }

    closedir(d);
    return found;
}

int main(int argc, char **argv) {
    // Args: [-f=png|jpg] [prefix]  (order-independent; defaults: png, "screenshot")
    const char *out_prefix = "screenshot";
    const char *fmt = "png";
    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "-f=", 3) == 0)
            fmt = argv[i] + 3;
        else
            out_prefix = argv[i];
    }
    const int is_jpg = (strcmp(fmt, "jpg") == 0 || strcmp(fmt, "jpeg") == 0);
    if (!is_jpg && strcmp(fmt, "png") != 0) {
        fprintf(stderr, "unknown format '%s' (use -f=png or -f=jpg)\n", fmt);
        return 2;
    }
    const char *ext = is_jpg ? "jpg" : "png";

    ds_drm drm;
    drm.drm_fd = -1;
    ds_kms_result result;
    memset(&result, 0, sizeof(result));
    char card[256] = {0};

    if (find_and_capture(&drm, &result, card, sizeof(card)) != 0) {
        fprintf(stderr, "no capturable KMS plane found "
                        "(are you running as root, and is a display active?)\n");
        return 1;
    }
    printf("using %s: %d plane(s)\n", card, result.num_items);

    ds_egl *egl = ds_egl_create(NULL);
    if (!egl) {
        fprintf(stderr, "failed to init EGL\n");
        return 1;
    }

    int saved = 0;
    for (int i = 0; i < result.num_items; ++i) {
        ds_kms_item *it = &result.items[i];

        ds_dmabuf_desc desc = {0};
        desc.fourcc = it->pixel_format;
        desc.width = it->width;
        desc.height = it->height;
        desc.modifier = it->modifier;
        desc.num_planes = it->num_dma_bufs;
        for (int j = 0; j < it->num_dma_bufs && j < 4; ++j) {
            desc.fds[j] = it->dma_buf[j].fd;
            desc.offsets[j] = it->dma_buf[j].offset;
            desc.pitches[j] = it->dma_buf[j].pitch;
        }

        printf("plane %d: %ux%u fourcc=%.4s mod=0x%llx planes=%d %s\n",
               i, it->width, it->height, (const char *)&it->pixel_format,
               (unsigned long long)it->modifier, it->num_dma_bufs,
               it->is_cursor ? "(cursor)" : "");

        uint8_t *rgba = NULL;
        uint32_t w = 0, h = 0;
        if (ds_egl_capture_dmabuf(egl, &desc, &rgba, &w, &h) != 0) {
            fprintf(stderr, "plane %d: capture failed\n", i);
            continue;
        }

        char path[512];
        snprintf(path, sizeof(path), "%s-%d%s.%s", out_prefix, i,
                 it->is_cursor ? "-cursor" : "", ext);
        const int rc = is_jpg ? ds_save_jpeg(path, rgba, w, h, 90)
                              : ds_save_png(path, rgba, w, h);
        if (rc == 0) {
            printf("  -> %s\n", path);
            saved++;
        }
        free(rgba);
    }

    // Release dma-buf fds.
    for (int i = 0; i < result.num_items; ++i)
        for (int j = 0; j < result.items[i].num_dma_bufs; ++j)
            if (result.items[i].dma_buf[j].fd >= 0)
                close(result.items[i].dma_buf[j].fd);

    ds_egl_destroy(egl);
    close(drm.drm_fd);

    printf("saved %d image(s)\n", saved);
    return saved > 0 ? 0 : 1;
}
