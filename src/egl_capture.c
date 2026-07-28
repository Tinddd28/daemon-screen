#include "../include/egl_capture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <drm_fourcc.h>
#include <png.h>
#include <jpeglib.h>

// The framebuffer we sample from is tiled/compressed (e.g. AMD DCC) and may be
// XRGB2101010 etc., so we cannot mmap it directly. We import it as an external
// EGL texture and let the GPU detile/convert while we blit it into a plain
// RGBA8 FBO, which we can then read back linearly.

static const char *vert_src =
    "attribute vec2 pos;\n"
    "varying vec2 v_uv;\n"
    "void main() {\n"
    "    v_uv = vec2(pos.x * 0.5 + 0.5, pos.y * 0.5 + 0.5);\n"
    "    gl_Position = vec4(pos, 0.0, 1.0);\n"
    "}\n";

static const char *frag_src =
    "#extension GL_OES_EGL_image_external : require\n"
    "precision highp float;\n"
    "varying vec2 v_uv;\n"
    "uniform samplerExternalOES tex;\n"
    "void main() {\n"
    "    gl_FragColor = texture2D(tex, v_uv);\n"
    "}\n";

static GLuint compile_shader(GLenum type, const char *src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(sh, sizeof(log), NULL, log);
        fprintf(stderr, "shader compile failed: %s\n", log);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

static int build_program(ds_egl *e) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vert_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag_src);
    if (!vs || !fs)
        return -1;

    e->program = glCreateProgram();
    glAttachShader(e->program, vs);
    glAttachShader(e->program, fs);
    glLinkProgram(e->program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(e->program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(e->program, sizeof(log), NULL, log);
        fprintf(stderr, "program link failed: %s\n", log);
        return -1;
    }

    e->attr_pos = glGetAttribLocation(e->program, "pos");
    e->uni_tex = glGetUniformLocation(e->program, "tex");

    // Fullscreen quad as a triangle strip.
    static const GLfloat quad[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f,
    };
    glGenBuffers(1, &e->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, e->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    return 0;
}

ds_egl *ds_egl_create(const char *render_node) {
    if (!render_node)
        render_node = "/dev/dri/renderD128";

    ds_egl *e = calloc(1, sizeof(*e));
    if (!e)
        return NULL;
    e->render_fd = -1;

    e->render_fd = open(render_node, O_RDWR | O_CLOEXEC);
    if (e->render_fd < 0) {
        fprintf(stderr, "failed to open render node %s: %s\n", render_node, strerror(errno));
        goto fail;
    }

    e->gbm = gbm_create_device(e->render_fd);
    if (!e->gbm) {
        fprintf(stderr, "gbm_create_device failed\n");
        goto fail;
    }

    // Use the EXT entry point: epoxy can't resolve the EGL 1.5 core
    // eglGetPlatformDisplay before a display (and thus a version) exists.
    e->dpy = eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, e->gbm, NULL);
    if (e->dpy == EGL_NO_DISPLAY) {
        fprintf(stderr, "eglGetPlatformDisplayEXT failed\n");
        goto fail;
    }

    EGLint major = 0, minor = 0;
    if (!eglInitialize(e->dpy, &major, &minor)) {
        fprintf(stderr, "eglInitialize failed: 0x%x\n", eglGetError());
        goto fail;
    }

    const char *exts = eglQueryString(e->dpy, EGL_EXTENSIONS);
    if (!exts || !strstr(exts, "EGL_EXT_image_dma_buf_import")) {
        fprintf(stderr, "EGL_EXT_image_dma_buf_import not supported\n");
        goto fail;
    }

    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        fprintf(stderr, "eglBindAPI failed\n");
        goto fail;
    }

    // We never create a surface (FBO-only), so don't constrain EGL_SURFACE_TYPE;
    // GBM configs advertise EGL_WINDOW_BIT and would reject EGL_PBUFFER_BIT.
    const EGLint cfg_attr[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE,
    };
    EGLConfig config;
    EGLint num_config = 0;
    if (!eglChooseConfig(e->dpy, cfg_attr, &config, 1, &num_config) || num_config < 1) {
        fprintf(stderr, "eglChooseConfig failed\n");
        goto fail;
    }

    const EGLint ctx_attr[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE,
    };
    e->ctx = eglCreateContext(e->dpy, config, EGL_NO_CONTEXT, ctx_attr);
    if (e->ctx == EGL_NO_CONTEXT) {
        fprintf(stderr, "eglCreateContext failed: 0x%x\n", eglGetError());
        goto fail;
    }

    // Surfaceless: no draw/read surface, we render to an FBO.
    if (!eglMakeCurrent(e->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, e->ctx)) {
        fprintf(stderr, "eglMakeCurrent (surfaceless) failed: 0x%x\n", eglGetError());
        goto fail;
    }

    if (build_program(e) != 0)
        goto fail;

    return e;

fail:
    ds_egl_destroy(e);
    return NULL;
}

void ds_egl_destroy(ds_egl *e) {
    if (!e)
        return;
    if (e->dpy != EGL_NO_DISPLAY) {
        // Delete GL objects while the context is still current (epoxy needs a
        // current context to resolve GL entry points), then tear it down.
        if (e->ctx != EGL_NO_CONTEXT) {
            if (e->vbo)
                glDeleteBuffers(1, &e->vbo);
            if (e->program)
                glDeleteProgram(e->program);
            eglMakeCurrent(e->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroyContext(e->dpy, e->ctx);
        }
        eglTerminate(e->dpy);
    }
    if (e->gbm)
        gbm_device_destroy(e->gbm);
    if (e->render_fd >= 0)
        close(e->render_fd);
    free(e);
}

int ds_egl_capture_dmabuf(ds_egl *e, const ds_dmabuf_desc *desc,
                          uint8_t **out_rgba, uint32_t *out_w, uint32_t *out_h) {
    if (!e || !desc || !out_rgba)
        return -EINVAL;
    if (desc->num_planes < 1 || desc->num_planes > 4)
        return -EINVAL;

    // Build the dma-buf import attribute list. Plane attribute enums are laid
    // out in a regular stride, so index them arithmetically.
    EGLAttrib attr[64];
    int n = 0;
    attr[n++] = EGL_WIDTH;             attr[n++] = (EGLint)desc->width;
    attr[n++] = EGL_HEIGHT;            attr[n++] = (EGLint)desc->height;
    attr[n++] = EGL_LINUX_DRM_FOURCC_EXT; attr[n++] = (EGLint)desc->fourcc;

    const EGLint fd_enum[4]   = { EGL_DMA_BUF_PLANE0_FD_EXT, EGL_DMA_BUF_PLANE1_FD_EXT,
                                  EGL_DMA_BUF_PLANE2_FD_EXT, EGL_DMA_BUF_PLANE3_FD_EXT };
    const EGLint off_enum[4]  = { EGL_DMA_BUF_PLANE0_OFFSET_EXT, EGL_DMA_BUF_PLANE1_OFFSET_EXT,
                                  EGL_DMA_BUF_PLANE2_OFFSET_EXT, EGL_DMA_BUF_PLANE3_OFFSET_EXT };
    const EGLint pit_enum[4]  = { EGL_DMA_BUF_PLANE0_PITCH_EXT, EGL_DMA_BUF_PLANE1_PITCH_EXT,
                                  EGL_DMA_BUF_PLANE2_PITCH_EXT, EGL_DMA_BUF_PLANE3_PITCH_EXT };
    const EGLint mlo_enum[4]  = { EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT,
                                  EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT };
    const EGLint mhi_enum[4]  = { EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT,
                                  EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT };

    const bool have_mod = desc->modifier != DRM_FORMAT_MOD_INVALID;
    for (int i = 0; i < desc->num_planes; ++i) {
        attr[n++] = fd_enum[i];  attr[n++] = desc->fds[i];
        attr[n++] = off_enum[i]; attr[n++] = (EGLint)desc->offsets[i];
        attr[n++] = pit_enum[i]; attr[n++] = (EGLint)desc->pitches[i];
        if (have_mod) {
            attr[n++] = mlo_enum[i]; attr[n++] = (EGLint)(desc->modifier & 0xffffffff);
            attr[n++] = mhi_enum[i]; attr[n++] = (EGLint)(desc->modifier >> 32);
        }
    }
    attr[n++] = EGL_NONE;

    EGLImage img = eglCreateImage(e->dpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
                                  (EGLClientBuffer)NULL, attr);
    if (img == EGL_NO_IMAGE) {
        fprintf(stderr, "eglCreateImage(dma_buf) failed: 0x%x "
                        "(fourcc=%.4s mod=0x%llx planes=%d)\n",
                eglGetError(), (const char *)&desc->fourcc,
                (unsigned long long)desc->modifier, desc->num_planes);
        return -1;
    }

    int rc = -1;
    GLuint tex = 0, fbo = 0, color = 0;
    uint8_t *pixels = NULL;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, tex);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, (GLeglImageOES)img);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // RGBA8 destination we can read back.
    glGenTextures(1, &color);
    glBindTexture(GL_TEXTURE_2D, color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, desc->width, desc->height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "FBO incomplete: 0x%x\n", glCheckFramebufferStatus(GL_FRAMEBUFFER));
        goto out;
    }

    glViewport(0, 0, desc->width, desc->height);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(e->program);
    glBindBuffer(GL_ARRAY_BUFFER, e->vbo);
    glEnableVertexAttribArray(e->attr_pos);
    glVertexAttribPointer(e->attr_pos, 2, GL_FLOAT, GL_FALSE, 0, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, tex);
    glUniform1i(e->uni_tex, 0);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    pixels = malloc((size_t)desc->width * desc->height * 4);
    if (!pixels) {
        rc = -ENOMEM;
        goto out;
    }
    glReadPixels(0, 0, desc->width, desc->height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    GLenum gl_err = glGetError();
    if (gl_err != GL_NO_ERROR) {
        fprintf(stderr, "GL error during capture: 0x%x\n", gl_err);
        free(pixels);
        pixels = NULL;
        goto out;
    }

    *out_rgba = pixels;
    if (out_w) *out_w = desc->width;
    if (out_h) *out_h = desc->height;
    rc = 0;

out:
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (fbo)   glDeleteFramebuffers(1, &fbo);
    if (color) glDeleteTextures(1, &color);
    if (tex)   glDeleteTextures(1, &tex);
    eglDestroyImage(e->dpy, img);
    return rc;
}

int ds_save_png(const char *path, const uint8_t *rgba, uint32_t w, uint32_t h) {
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        fprintf(stderr, "failed to open %s: %s\n", path, strerror(errno));
        return -1;
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) { fclose(fp); return -1; }
    png_infop info = png_create_info_struct(png);
    if (!info) { png_destroy_write_struct(&png, NULL); fclose(fp); return -1; }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        fclose(fp);
        return -1;
    }

    png_init_io(png, fp);
    png_set_IHDR(png, info, w, h, 8, PNG_COLOR_TYPE_RGBA,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    // Our sampling maps texture v=0 (source top row) to the bottom of the FBO,
    // and glReadPixels is bottom-origin, so read-back row 0 is already the top
    // of the source. Write rows in natural order for an upright image.
    for (uint32_t y = 0; y < h; ++y)
        png_write_row(png, (png_bytep)(rgba + (size_t)y * w * 4));

    png_write_end(png, info);
    png_destroy_write_struct(&png, &info);
    fclose(fp);
    return 0;
}

int ds_save_jpeg(const char *path, const uint8_t *rgba, uint32_t w, uint32_t h, int quality) {
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        fprintf(stderr, "failed to open %s: %s\n", path, strerror(errno));
        return -1;
    }

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, fp);

    cinfo.image_width = w;
    cinfo.image_height = h;
    // Feed RGBA directly: libjpeg-turbo's JCS_EXT_RGBA reads 4 bytes/pixel and
    // ignores the alpha channel, so no manual RGBA->RGB pass is needed.
    cinfo.input_components = 4;
    cinfo.in_color_space = JCS_EXT_RGBA;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);

    jpeg_start_compress(&cinfo, TRUE);
    // Same row order as PNG: read-back row 0 is the top of the source.
    while (cinfo.next_scanline < h) {
        JSAMPROW row = (JSAMPROW)(rgba + (size_t)cinfo.next_scanline * w * 4);
        jpeg_write_scanlines(&cinfo, &row, 1);
    }
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    fclose(fp);
    return 0;
}
