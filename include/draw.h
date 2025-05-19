#ifndef DRAW_H
#define DRAW_H
#include <EGL/egl.h>
#include <stdbool.h>
#include <kms.h>
#include <gbm.h>

void render_dma_buf(EGLDisplay display, EGLImage image);
EGLImage ds_create_egl_image(EGLDisplay display,ds_kms_item *drm_fd, const int *fds, uint32_t format, uint32_t width, uint32_t height, const uint32_t *offsets, const uint32_t *pitches, const uint32_t *modifies, int num_planes, bool use_modifier); 
EGLDisplay init_egl_display(int drm_fd, struct gbm_device **out_gbm);
void render_item(ds_kms_item *item, ds_drm *drm);
#endif /* DRAW_H */