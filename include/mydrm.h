#pragma once

#include <libdrm/drm.h>
#include <libdrm/drm_mode.h>

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

struct  drm_get_capacity {
    uint64_t capability;
    uint64_t value;
};

enum drm_mode_status {
    DRM_MODE_CONNECTED = 1,
    DRM_MODE_DISCONNECTED = 2,
    DRM_MODE_UNKNOWN = 3
};

extern int drm_ioctl(int fd, unsigned long request, void *arg);
extern int drm_open(const char *drm_device);
extern int drm_get_resources(int fd, struct drm_mode_card_res *res);
extern int drm_get_connector(int fd, int id, struct drm_mode_get_connector *conn);