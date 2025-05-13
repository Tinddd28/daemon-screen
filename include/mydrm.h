#ifndef MYDRM_H
#define MYDRM_H
#pragma once
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

struct drm_device {
    struct drm_device *next;

    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t size;
    uint32_t handle;
    uint8_t *map;

    drmModeModeInfo mode;
    uint32_t fb_id;
    uint32_t conn_id;
    uint32_t crtc_id;
    drmModeCrtc *saved_crtc;
};


extern struct drm_device *drm_devices_list;
extern int drm_open(int *out, const char *path);
extern int drm_device_prepare(int fd);
extern int drm_device_setup(int fd, drmModeRes *res, drmModeConnector *conn, struct drm_device *dev);
extern int drm_device_find_crtc(int fd, drmModeRes *res, drmModeConnector *conn, struct drm_device *dev);
extern int drm_device_create_fb(int fd, struct drm_device *dev);
extern int drm_device_destroy_fb(int fd, struct drm_device *dev);

#endif