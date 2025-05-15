#include <kms.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>

int main() {
    const char *card = "/dev/dri/card0";

    ds_drm drm;
    drm.drm_fd = 0;

    if (open_drm_device(card, &drm)) {
        printf("failed to open drm device %s", strerror(errno));
        return -errno;
    }

    ds_kms_result result;
    result.num_items = 0;
    printf("hello\n");
    if (kms_get_fb(&drm, &result) == 0) {
        printf("something happened");
    }
    printf("after kms_get_fb\n");
    print_result(&result);

    res(&result, drm);
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