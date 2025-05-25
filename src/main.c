#include <draw.h>
#include <utils.h>
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


    

    for (int i = 0; i < result.num_items; ++i) {
        ds_kms_item *item = &result.items[i];
        for (int j = 0; j < result.items[i].num_dma_bufs; ++j) {
            ds_kms_dma_buf *dma_buf = &item->dma_buf[j];
            if (dma_buf->fd > 0) {
                // close(dma_buf->fd);
                // dma_buf->fd = -1;
                if (fcntl(dma_buf->fd, F_GETFD) == -1) {
    perror("Invalid DMA-BUF FD");
    continue;
}
                size_t size = item->height * dma_buf->pitch;
                void *data = map_dma_buf(dma_buf->fd, size, dma_buf->offset);   
                if (!data) {
                    fprintf(stderr, "failed to map\n");
                    continue;
                }

                uint8_t *pixel_data = (uint8_t *)data;
            uint8_t r = pixel_data[2]; // Красный канал (RGB)
            uint8_t g = pixel_data[1]; // Зеленый канал
            uint8_t b = pixel_data[0]; // Синий канал
            uint8_t a = pixel_data[3]; // Альфа-канал (если есть)

            printf("\t\t\tFirst Pixel: R=%u; G=%u; B=%u; A=%u\n", r, g, b, a);

            // Демаппинг буфера
            unmap_dma_buf(data, size, dma_buf->offset);
            }

        }
        // result.items[i].num_dma_bufs = 0;
    }
    // result.num_items = 0;

    
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
