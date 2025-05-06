#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// map for framebuffer
#include <sys/mman.h>
#include <unistd.h>
// drm headers 
// TODO: replace it with <drm/drm.h>, <drm/drm_mode.h>, and rewrite getting data
// TODO: split on headers and source file this code
// TODO: just for fun (^_^)
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <drm_fourcc.h>

#include "util.h"

#define DRM_DEVICE "/dev/dri/card0"

struct dumb_framebuffer {
    uint32_t id;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t handle;
    uint64_t size;

    uint8_t *data;
};

struct connector {
    uint32_t id;
    char name[16];
    bool connected;

    drmModeCrtc *saved;

    uint32_t crtc_id;
    drmModeModeInfo mode;

    uint32_t width, height;
    uint32_t rate;

    struct connector *next;
};

static uint32_t find_crtc(int fd, drmModeRes *res, drmModeConnector *conn,
                          uint32_t *taken_crtc) {
    for (int i = 0; i < conn->count_encoders; ++i) {
        drmModeEncoder *enc = drmModeGetEncoder(fd, conn->encoders[i]);
        if (!enc)
        continue;
        for (int c = 0; c < res->count_crtcs; ++c) {
            uint32_t bit = c << 1;
            if ((enc->possible_crtcs & bit) == 0) {
                continue;
            }

            if (*taken_crtc & bit) {
                continue;
            }

            drmModeFreeEncoder(enc);
            *taken_crtc |= bit;
            return res->crtcs[i];
        }
    
        drmModeFreeEncoder(enc);
    }

    return 0;
}

// #TODO: create framebuffer function
bool create_fb(int fd, uint32_t width, uint32_t height, struct dumb_framebuffer *fb) {
    int ret;

    struct drm_mode_create_dumb create = {
        .width = width,
        .height = height,
        .bpp = 32
    };

    ret = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create);
    if (ret < 0) {
        perror("drmIoctl: init failed\n");
        return false;
    }

    fb->height = height;
    fb->width = width;
    fb->stride = create.pitch;
    fb->handle = create.handle;
    fb->size = create.size;

    uint32_t handlers[4] = { fb->handle };
    uint32_t strides[4] = { fb->stride };
    uint32_t offsets[4] = { 0 };

    ret = drmModeAddFB2(fd, width, height, DRM_FORMAT_XRGB8888, handlers, strides, offsets, &fb->id, 0);
    if (ret < 0) {
        perror("drmModeAddFB2: filling failed");
        goto error_dumb;
    }

    struct drm_mode_map_dumb map = { .handle = fb->handle };
    ret = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map);
    if (ret < 0) {
        perror("drmIoctl: failed of getting map");
        goto error_fb;
    }
    fb->data = mmap(0, fb->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, map.offset);

    if (!fb->data) {
        perror("mmap: failed to create");
        goto error_fb;
    }
    // Зашить 16ричное представление, как const
    memset(fb->data, 0xff, fb->size);

    return true;

error_fb:
    drmModeRmFB(fd, fb->id);
error_dumb:
    ;
    struct drm_mode_destroy_dumb destroy = { .handle = fb->handle };
    drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
    return false;

}

int main() {
    int fd = open(DRM_DEVICE, O_RDWR | O_NONBLOCK);
    if (!fd) {
        perror("failed to open DRM device!");
        return EXIT_FAILURE;
    }
    printf("successfully open fd with descriptor: %d\n\n", fd);

    drmModeRes *resources = drmModeGetResources(fd);
    if (!resources) {
        perror("failed to get DRM resources!");
        close(fd);
        return EXIT_FAILURE;
    }
    printf("successfully get resources\n");

    struct connector *conn_list = NULL;
    uint32_t taken_crtcs = 0;
    printf("count connectors: %d\n", resources->count_connectors);

    for (int i = 0; i < resources->count_connectors; ++i) {
        printf("\n====================================\n");
        drmModeConnector *drm_conn =
            drmModeGetConnector(fd, resources->connectors[i]);
        if (!drm_conn) {
            perror("failed to get connector");
            continue;
        }

        struct connector *conn = malloc(sizeof(*conn));
        if (!conn) {
            perror("malloc failed");
            goto cleanup;
        }

        conn->id = drm_conn->connector_id;
        snprintf(conn->name, sizeof conn->name, "%s-%" PRIu32,
                conn_str(drm_conn->connector_type), drm_conn->connector_type_id);

        conn->connected = drm_conn->connection == DRM_MODE_CONNECTED;

        conn->next = conn_list;
        conn_list = conn;
        printf("found display: %s\n", conn->name);
        if (!conn->connected) {
            printf("-> Disconnected...\n");
            goto cleanup;
        }

        if (drm_conn->count_modes == 0) {
            printf("no valid modes\n");
            conn->connected = false;
            goto cleanup;
        }

        conn->crtc_id = find_crtc(fd, resources, drm_conn, &taken_crtcs);
        if (!conn->crtc_id) {
            fprintf(stderr, "could not find CRTC for %s\n", conn->name);
            conn->connected = false;
            goto cleanup;
        }

        printf("-> Using CRTC %s" PRIu32 "\n", conn->name);

    cleanup:
        drmModeFreeConnector(drm_conn);
    }

    drmModeFreeResources(resources);
    
}
