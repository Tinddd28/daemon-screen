#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/mman.h> 
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <inttypes.h>

#include "util.h"

#define DRM_DEVICE "/dev/dri/card0"

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

static uint32_t find_crtc(int fd, drmModeRes *res, drmModeConnector *conn, uint32_t *taken_crtc) {
    for (int i = 0; i < conn->count_encoders; ++i) {
        drmModeEncoder *enc = drmModeGetEncoder(fd, conn->encoders[i]);
        if (!enc) 
            continue;
        for  (int c = 0; c < res->count_crtcs; ++c) {
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

int main() {
    int fd = open(DRM_DEVICE, O_RDWR | O_NONBLOCK);
    if (!fd) {
        perror("failed to open DRM device!");
        return EXIT_FAILURE;
    } 
    printf("successfully open fd with descriptor: %d\n", fd);

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
        printf("start cycle\n");
        drmModeConnector *drm_conn = drmModeGetConnector(fd, resources->connectors[i]);
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
        snprintf(conn->name, sizeof conn->name, "%s-%s"PRIu32,
            conn_str(drm_conn->connector_type),
            drm_conn->connector_type_id);


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

        printf("-> Using CRTC %s"PRIu32"\n", conn->crtc_id);



        // if (drm_conn && drm_conn->connection == DRM_MODE_CONNECTED && drm_conn->count_modes > 0) 
        // {
        //     printf("connector_type: %s", drmModeGetConnectorTypeName(drm_conn->connector_type));
        //     printf("\n\n======================================================\n");
        // }
cleanup:
        drmModeFreeConnector(drm_conn);
       
    }

    drmModeFreeResources(resources);

    // printf("screen resolution: %dx%d\n", crtc->width, crtc->height);

}