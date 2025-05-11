#include <mydrm.h>
#include <kms.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>

int main() {

    int ret, fd;
    const char *card = "/dev/dri/card0";
    struct drm_device *iter;
    ret = drm_open(&fd, card);
    if (ret) {
        errno = -ret;
        fprintf(stderr, "failed to open drm device %s: %s\n", card, strerror(errno));
        return ret;
    }
    // ret = drmSetMaster(fd);
    // if (ret) {
    //     fprintf(stderr, "failed to set drm master: %s\n", strerror(errno));
    //     close(fd);
    //     return ret;
    // }
    ret = drm_device_prepare(fd);
    if (ret) 
        close(fd);

    for (iter = drm_devices_list; iter; iter = iter->next) {
        if (!iter) {
            fprintf(stderr, "Invalid device in list\n");
            continue;
        }

        iter->saved_crtc = drmModeGetCrtc(fd, iter->crtc_id);
        if (!iter->saved_crtc) {
            fprintf(stderr, "Failed to get CRTC for connector %u\n", iter->conn_id);
            continue;
        }

        // ret = drmModeSetCrtc(fd, iter->crtc_id, iter->fb_id,
        //     0, 0, &iter->conn_id, 1, &iter->mode);
        // if (ret) {
        //     fprintf(stderr, "failed to set CRTC to connector %u (%s)\n",
        //         iter->conn_id, strerror(errno));
        //     continue;
        // }
    }

    // if (drm_devices_list) {
    //     save_framebuffer("./test.bmp", drm_devices_list);
    // }
    
    return ret;
}