#include <mydrm.h>
#include <usage.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef O_CLOEXEC
#define O_CLOEXEC 02000000
#endif 

int drm_ioctl(int fd, unsigned long request, void *arg) {
    int ret;

    do {
        ret = sys_ioctl(fd, request, arg);
    } while (ret == -EINTR || ret == -EAGAIN);

    return ret;
}
// usually drm_device is /dev/dri/card0 (if using only 1 one gpu)
int drm_open(const char *drm_device) {
    int fd = sys_open((char *)drm_device, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        return fd;
    }

    struct drm_get_capacity get_cap = {
        .capability = DRM_CAP_DUMB_BUFFER,
        .value = 0
    };

    if (drm_ioctl(fd, DRM_IOCTL_GET_CAP, &get_cap) < 0 || !get_cap.value) {
        return -EOPNOTSUPP;
    }

    return fd;
}


int drm_get_resources(int fd, struct drm_mode_card_res *res) {
    mem_set(res, 0, sizeof(struct drm_mode_card_res));

    int ior = 0;
    if (drm_ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, res)) {
        return -1;
    }

    if (res->count_fbs) {
        res->fb_id_ptr = (uint64_t)malloc(res->count_fbs * sizeof(uint32_t));
        mem_set((void *)res->fb_id_ptr, 0, res->count_fbs * sizeof(uint32_t));
    }
    if (res->count_connectors) {
        res->connector_id_ptr = (uint64_t)malloc(res->count_connectors * sizeof(uint32_t));
        mem_set((void *)res->connector_id_ptr, 0, res->count_connectors * sizeof(uint32_t));
    }
    if (res->count_crtcs) {
        res->crtc_id_ptr = (uint64_t)malloc(res->count_crtcs * sizeof(uint32_t));
        mem_set((void *)res->crtc_id_ptr, 0, res->count_crtcs * sizeof(uint32_t));
    }
    if (res->count_encoders) {
        res->encoder_id_ptr = (uint64_t)malloc(res->count_encoders * sizeof(uint32_t));
        mem_set((void *)res->encoder_id_ptr, 0, res->count_encoders * sizeof(uint32_t));
    }


    ior = drm_ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, res);
    if (ior) {
        return -1;
    }
    return 0;
}

int drm_get_connector(int fd, int id, struct drm_mode_get_connector *conn) {
    mem_set(conn, 0, sizeof(struct drm_mode_get_connector));

    conn->connector_id = id;


    if (drm_ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, conn)) {
        return -1;
    }

    if (conn->count_props) {
        conn->props_ptr = (uint64_t)malloc(conn->count_props * sizeof(uint32_t));
        conn->prop_values_ptr = (uint64_t)malloc(conn->count_props * sizeof(uint32_t));
    }

    if (conn->count_modes) {
        conn->modes_ptr = (uint64_t)malloc(conn->count_modes * sizeof(uint32_t));
    }
    
    if (conn->count_encoders) {
        conn->encoders_ptr = (uint64_t)malloc(conn->count_encoders * sizeof(uint32_t));
    }

    if (drm_ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, conn)) {
        return -1;
    }
    return 0;
}