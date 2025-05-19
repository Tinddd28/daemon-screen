#include "../include/draw.h"
#include "../include/utils.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <gbm.h>

EGLDisplay init_egl_display(int drm_fd, struct gbm_device **out_gbm) {
    struct gbm_device *gbm = gbm_create_device(drm_fd);

    if (!gbm) {
        fprintf(stderr, "failed to creat GBM device\n");
        return EGL_NO_DISPLAY;
    }

    if (out_gbm) *out_gbm = gbm;

    PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT =
        (void *)eglGetProcAddress("eglGetPlatformDisplayEXT");
    if (!eglGetPlatformDisplayEXT) {
        fprintf(stderr, "eglGetPlatformDisplayEXT not available\n");
        gbm_device_destroy(gbm);
        return EGL_NO_DISPLAY;
    }

    // 3. Get EGLDisplay for GBM
    EGLDisplay egl_display = eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, gbm, NULL);
    if (egl_display == EGL_NO_DISPLAY) {
        fprintf(stderr, "Failed to get EGLDisplay from GBM\n");
        gbm_device_destroy(gbm);
        return EGL_NO_DISPLAY;
    }

    // 4. Initialize EGL
    if (!eglInitialize(egl_display, NULL, NULL)) {
        fprintf(stderr, "Failed to initialize EGL\n");
        gbm_device_destroy(gbm);
        return EGL_NO_DISPLAY;
    }

    return egl_display;
}

EGLImage ds_create_egl_image(EGLDisplay display,ds_kms_item *drm_fd, const int *fds, uint32_t format, uint32_t width, uint32_t height, const uint32_t *offsets, const uint32_t *pitches, const uint32_t *modifies, int num_planes, bool use_modifier) {
    intptr_t attrs[44];
    setup_dma_buf_attrs(attrs, format, width, height, fds, offsets, pitches, modifies, num_planes, use_modifier);
    printf("suka \n");
    EGLImage image = eglCreateImage(display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, (const EGLint *)attrs);
    if (image == EGL_NO_IMAGE) {
        fprintf(stderr, "Failed to create EGL image: %x\n", eglGetError());
        return EGL_NO_IMAGE;
    }
    return image;
}

void render_dma_buf(EGLDisplay display, EGLImage image) {
    EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };

    EGLConfig config;
    EGLint num_configs;
    if (!eglChooseConfig(display, config_attribs, &config, 1, &num_configs)) {
        fprintf(stderr, "Failed to choose EGL config\n");
        eglTerminate(display);
        return ;
    }

    if (num_configs == 0) {
        fprintf(stderr, "No suitable EGL configs found\n");
        eglTerminate(display);
        return ;
    }

    // 4. Создание PBuffer поверхности
    EGLint pbuffer_attribs[] = {
        EGL_WIDTH, 1,
        EGL_HEIGHT, 1,
        EGL_NONE
    };



    EGLSurface surface = eglCreatePbufferSurface(display, config, pbuffer_attribs);
    if (surface == EGL_NO_SURFACE) {
        fprintf(stderr, "Failed to create PBuffer surface\n");
        eglTerminate(display);
        return -1;
    }
    EGLint ctx_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    EGLContext ctx = eglCreateContext(display, config, EGL_NO_CONTEXT, ctx_attribs);
    if (ctx == EGL_NO_CONTEXT) {
        fprintf(stderr, "Failed to create EGL context\n");
        eglTerminate(display);
        return;
    }
    if (!eglMakeCurrent(display, surface, surface, ctx)) {
        fprintf(stderr, "Failed to make EGL context current\n");
        eglDestroyContext(display, ctx);
        eglTerminate(display);
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    
   PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES =
        (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    if (glEGLImageTargetTexture2DOES)
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
    else
        fprintf(stderr, "glEGLImageTargetTexture2DOES not available\n"); 

    // 3. Теперь можно рисовать эту текстуру стандартными средствами OpenGL
    // (создайте шейдеры, VBO, нарисуйте quad и т.д.)

    // 4. Очистка
    glDeleteTextures(1, &tex);
    eglDestroyContext(display, ctx);
}

void render_item(ds_kms_item *item, ds_drm *drm) {
    int fds[DS_KMS_MAX_DMA_BUFS];
    uint32_t offsets[DS_KMS_MAX_DMA_BUFS];
    uint32_t pitches[DS_KMS_MAX_DMA_BUFS];
    uint32_t modifies[DS_KMS_MAX_DMA_BUFS];
    printf("hello motherfacker\n");
    for (int i = 0; i < item->num_dma_bufs; ++i) {
        ds_kms_dma_buf *dma_buf = &item->dma_buf[i];
        fds[i] = dma_buf->fd;
        offsets[i] = dma_buf->offset;
        pitches[i] = dma_buf->pitch;
        modifies[i] = 0;
    }
    struct gbm_device *gbm = NULL;
    EGLDisplay display = init_egl_display(drm->drm_fd, &gbm);
    if (display == EGL_NO_DISPLAY) {
        fprintf(stderr, "Failed to initialize EGL display\n");
        return ;
    }

    printf("EGL display initialized successfully\n");
    EGLImage image = ds_create_egl_image(display, item, fds, item->pixel_format, item->width, item->height, offsets, pitches, modifies, item->num_dma_bufs, false);
    if (image == EGL_NO_IMAGE) {
        fprintf(stderr, "Failed to create EGL image\n");
        return;
    }
    printf("EGL image created successfully\n");
    
    render_dma_buf(display, image);

    eglDestroyImage(display, image);
    gbm_device_destroy(gbm);
}