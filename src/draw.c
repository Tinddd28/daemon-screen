// #include <../include/draw.h>
#include <../include/kms.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

EGLDisplay init_egl(int drm_fd) {
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        fprintf(stderr, "Failed to get EGL display\n");
        return EGL_NO_DISPLAY;
    }

    if (!eglInitialize(display, NULL, NULL)) {
        fprintf(stderr, "failed to initializate EGL\n");
        return EGL_NO_DISPLAY;
    }

    return display;
}


GLuint create_texture(EGLDisplay display, struct ds_kms_item *item) {
    GLuint *textures = malloc(item->num_dma_bufs * sizeof(GLuint));
    if (!textures) {
        fprintf(stderr, "failed to allcate memory for textures\n");
        return NULL;
    }
    for (int i = 0; i < item->num_dma_bufs; ++i) {
        ds_kms_dma_buf *dma_buf = &item->dma_buf[i];
        EGLint attrs[] = {
            EGL_WIDTH, item->width,
            EGL_HEIGHT, item->height,
            EGL_LINUX_DRM_FOURCC_EXT, item->pixel_format,
            EGL_DMA_BUF_PLANE0_FD_EXT, dma_buf->fd,
            EGL_DMA_BUF_PLANE0_PITCH_EXT, dma_buf->pitch,
            EGL_DMA_BUF_PLANE0_OFFSET_EXT, dma_buf->offset,
            EGL_NONE 
        };

        EGLImageKHR image = eglCreateImageKHR(display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attrs);
        if (image == EGL_NO_IMAGE_KHR) {
            frpintf(stderr, "failed to create EGLImage for DMA-BUF %d\n", i+1);
            free(textures);
            return NULL;
        }

        glGenTextures(1, &textures[i]);
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
    }
    return textures;
}

void render_texture(GLuint texture, uint32_t width, uint32_t height) {
    // Устанавливаем размеры окна
    glViewport(0, 0, width, height);

    // Очищаем экран
    glClear(GL_COLOR_BUFFER_BIT);

    // Привязываем текстуру
    glBindTexture(GL_TEXTURE_2D, texture);

    // Рисуем прямоугольник с текстурой
    glBegin(GL_QUADS_EXT);
    glTexCoord2f(0, 0); glVertex2f(-1, -1);
    glTexCoord2f(1, 0); glVertex2f(1, -1);
    glTexCoord2f(1, 1); glVertex2f(1, 1);
    glTexCoord2f(0, 1); glVertex2f(-1, 1);
    glEnd();
}


void res(ds_kms_result *res, ds_drm *drm) {
    EGLDisplay dis = init_egl(drm->drm_fd);
    if (!dis) {
        printf(stderr, "failed get display\n");
        return NULL;
    }

    GLuint textures = create_texture(dis, &res->items[0]);
    if (!textures) {
        printf(stderr, "failed to get textures\n");
        return NULL;
    }

    render_texture(textures, res->items[0].width, res->items[0].height);
}