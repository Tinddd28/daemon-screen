#include <mydrm.h>
#include <usage.h>

#include <stdlib.h>
#include <stdio.h>

int main() {
    printf("DRM modes:\n");

    int fd = drm_open("/dev/dri/card0");

    struct drm_mode_card_res res;

    if (drm_get_resources(fd, &res)) {
        printf("failed to open card resources\n");
        return -1;
    }

    printf("DRM connectors count: %d", res.count_connectors);
    

    return 0;
}