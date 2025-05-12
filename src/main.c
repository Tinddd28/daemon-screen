#include <mydrm.h>
#include <kms.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>

int main() {
    int res = 0;
    const char *card = "/dev/dri/card0";
    ds_drm drm;
    drm.drm_fd = 0;

    drm.drm_fd = open(card, O_RDONLY);
    if (drm.drm_fd < 0) {
        fprintf(stderr, "failed to open %s, error: %s", card, strerror(errno));
        res = 2;
        close(drm.drm_fd);
        return res;
    }
    printf("drm device opened\n");
    if (drmSetClientCap(drm.drm_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) != 0) {
        fprintf(stderr, "drmSetClientCap: DRM_CLIENT_CAP_UNIVERSAL_PLANES failed, error: %s\n", strerror(errno));
        res = 2;
        close(drm.drm_fd);
        return res;
    }

    if (drmSetClientCap(drm.drm_fd, DRM_CLIENT_CAP_ATOMIC, 1) != 0){
        fprintf(stderr, "drmSetClientCap DRM_CLIENT_CAP_ATOMIC failed, error: %s", strerror(errno));
    }

    ds_kms_result result;
    result.num_items = 0;
    printf("hello\n");
    if (kms_get_fb(&drm, &result) == 0) {
        printf("something happened");
    }

    for (int i = 0; i < result.num_items; ++i) {
        for (int j = 0; j < result.items[i].num_dma_bufs; ++j) {
            ds_kms_dma_buf *dma_buf = &result.items[i].dma_buf[j];
            if (dma_buf->fd > 0) {
                close(dma_buf->fd);
                dma_buf->fd = -1;
            }
        }
        result.items[i].num_dma_bufs = 0;
    }
    result.num_items = 0;

} 