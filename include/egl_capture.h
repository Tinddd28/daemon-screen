// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Shibakov Nikita <tind.nik.28@gmail.com>

#ifndef EGL_CAPTURE_H
#define EGL_CAPTURE_H

#include <stdint.h>
#include <stdbool.h>

#include <epoxy/egl.h>
#include <epoxy/gl.h>

#include <gbm.h>

// A minimal EGL/GBM context used to import KMS dma-buf framebuffers and read
// them back as linear RGBA. Runs headless (surfaceless) on a render node, so it
// needs no DRM master and no window system.
typedef struct {
    int render_fd;
    struct gbm_device *gbm;
    EGLDisplay dpy;
    EGLContext ctx;

    GLuint program;
    GLuint vbo;
    GLint attr_pos;
    GLint uni_tex;
} ds_egl;

// Description of one imported plane's dma-buf. Mirrors what kms_get_fb collects.
typedef struct {
    uint32_t fourcc;   // DRM_FORMAT_* of the source framebuffer
    uint32_t width;
    uint32_t height;
    uint64_t modifier; // DRM_FORMAT_MOD_INVALID if none / linear-unspecified
    int num_planes;
    int fds[4];
    uint32_t offsets[4];
    uint32_t pitches[4];
} ds_dmabuf_desc;

// Create the headless EGL context. render_node may be NULL -> "/dev/dri/renderD128".
// Returns NULL on failure (message on stderr).
ds_egl *ds_egl_create(const char *render_node);
void ds_egl_destroy(ds_egl *e);

// Import desc as an EGLImage, sample it into an RGBA8 FBO and read it back.
// On success *out_rgba is a malloc'd width*height*4 buffer (caller frees) and
// returns 0. Rows are top-to-bottom (row 0 = top of screen).
int ds_egl_capture_dmabuf(ds_egl *e, const ds_dmabuf_desc *desc,
                          uint8_t **out_rgba, uint32_t *out_w, uint32_t *out_h);

// Write an RGBA8 buffer (top-to-bottom) to a PNG file (lossless, keeps alpha).
int ds_save_png(const char *path, const uint8_t *rgba, uint32_t w, uint32_t h);

// Write an RGBA8 buffer (top-to-bottom) to a JPEG file (lossy; alpha discarded).
// quality is 0-100 (higher = better/larger).
int ds_save_jpeg(const char *path, const uint8_t *rgba, uint32_t w, uint32_t h, int quality);

#endif
