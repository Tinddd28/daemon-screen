#include <mydrm.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#ifndef O_CLOEXEC
#define O_CLOEXEC 02000000
#endif 

struct drm_device *drm_devices_list = NULL;

// usually drm_device is /dev/dri/card0 
int drm_open(int *out, const char *path) {
    int fd, ret;
    uint64_t has_dumb;

    fd = open((char *)path, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        ret = -errno;
        fprintf(stderr, "failed to open %s: %s\n", path, strerror(-ret));
        return ret;
    }

    if (drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &has_dumb) < 0 || !has_dumb) {
        fprintf(stderr, "drm device %s does not support dumb buffers\n", path);
        close(fd);
        return -EOPNOTSUPP;
    }

    *out = fd;

    return 0;
}

int drm_device_create_fb(int fd, struct drm_device *dev) {
    struct drm_mode_create_dumb creq;
    struct drm_mode_map_dumb mreq;
    int ret;

    memset(&creq, 0, sizeof(creq));
    creq.width = dev->width;
    creq.height = dev->height;
    creq.bpp = 32;
    ret = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);
    if (ret < 0) {
        fprintf(stderr, "failed to create dumb buffer: %s\n", strerror(errno));
        return errno;
    }
    dev->stride = creq.pitch;
    dev->size = creq.size;
    dev->handle = creq.handle;

    ret = drmModeAddFB(fd, dev->width, dev->height, 24, 32,
        dev->stride, dev->handle, &dev->fb_id);
    if (ret) {
        fprintf(stderr, "failed to add framebuffer: %s\n", strerror(errno));
        return drm_device_destroy_fb(fd, dev);
    }

    memset(&mreq, 0, sizeof(mreq));
    mreq.handle = dev->handle;
    ret = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq);
    if (ret) {
        fprintf(stderr, "failed to map dumb buffer: %s\n", strerror(errno));
        return drmModeRmFB(fd, dev->fb_id);
    }

    dev->map = mmap(0, dev->size, PROT_READ | PROT_WRITE, MAP_SHARED,
        fd, mreq.offset);
    
    if (dev->map == MAP_FAILED) {
       fprintf(stderr, "failed to mmap dumb buffer: %d\n", errno); 
       return drmModeRmFB(fd, dev->fb_id);
    }

    memset(dev->map, 0, dev->size);

    return 0;
}

int drm_device_destroy_fb(int fd, struct drm_device *dev) {
    struct drm_mode_destroy_dumb dreq;
    int ret;

    memset(&dreq, 0, sizeof(dreq));
    dreq.handle = dev->handle;
    ret = drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
    if (ret) {
        fprintf(stderr, "failed to destroy dumb buffer: %s\n", strerror(errno));
        return -errno;
    }

    return 0;
}

int drm_device_find_crtc(int fd, drmModeRes *res, drmModeConnector *conn, struct drm_device *dev) {
    drmModeEncoder *enc;
    unsigned int i, j;
    int32_t crtc;
    
    struct drm_device *iter;
    
    if (conn->encoder_id) 
        enc = drmModeGetEncoder(fd, conn->encoder_id);
    else 
        enc = NULL;

    if (enc) {
        if (enc->crtc_id) {
            crtc = enc->crtc_id;
            for (iter = drm_devices_list; iter; iter = iter->next) {
                if (iter->crtc_id == crtc) {
                    crtc = -1;
                    break;
                }
            }

            if (crtc >= 0) {
                dev->crtc_id = crtc;
                drmModeFreeEncoder(enc);
                return 0;
            }
        }
        drmModeFreeEncoder(enc);
    }

    for (i = 0; i < res->count_encoders; ++i) {
        enc = drmModeGetEncoder(fd, res->encoders[i]);
        if (!enc) {
            fprintf(stderr, "failed to get encoder %u\n", res->encoders[i]);
            continue;
        }

        for (j = 0; j < res->count_crtcs; ++j) {
            if (!(enc->possible_crtcs & (i << j))) {
                continue;
            }

            crtc = res->crtcs[j];
            for (iter = drm_devices_list; iter; iter = iter->next) {
                if (iter->crtc_id == crtc) {
                    crtc = -1;
                    break;
                }
            }

            if (crtc >= 0) {
                dev->crtc_id = crtc;
                drmModeFreeEncoder(enc);
                return 0;
            }
        }
        drmModeFreeEncoder(enc);
    }
    fprintf(stderr, "failed to find CRTC for connector %u\n",
        conn->connector_id);
    return -ENOENT;
}


int drm_device_setup(int fd, drmModeRes *res, drmModeConnector *conn, struct drm_device *dev) {
    int ret;

    if (conn->connection != DRM_MODE_CONNECTED) {
        fprintf(stderr, "connector %u is not connected\n", 
            conn->connector_id);
        return -ENOENT;
    }

    if (conn->count_modes == 0) {
        fprintf(stderr, "connector %u has no modes\n", 
            conn->connector_id);
        return -EFAULT;
    }

    memcpy(&dev->mode, &conn->modes[0], sizeof(dev->mode));
    dev->width = conn->modes[0].hdisplay;
    dev->height = conn->modes[0].vdisplay;
    fprintf(stderr, "mode for connector %u: %ux%u\n", 
        conn->connector_id, dev->width, dev->height);

    ret = drm_device_find_crtc(fd, res, conn, dev);
    if (ret) {
        fprintf(stderr, "failed to find crtc for connector %u\n",
        conn->connector_id);
        return ret;
    }

    ret = drm_device_create_fb(fd, dev);
    if (ret) {
        fprintf(stderr, "failed to create framebuffer for connector %u\n",
        conn->connector_id);
        return ret;
    };

    return 0;
}


int drm_device_prepare(int fd) {
    drmModeRes *res;
    drmModeConnector *conn;
    unsigned int i;
    struct drm_device *dev;
    int ret;

    res = drmModeGetResources(fd);
    if (!res) {
        fprintf(stderr, "can not get resources: %s\n", strerror(errno));
        return -errno;
    }


    for (i = 0; i < res->count_connectors; ++i) {
        conn = drmModeGetConnector(fd, res->connectors[i]);
        if (!conn) {
            fprintf(stderr, "can not get connector %u:%u (%d)\n", i, res->connectors[i], errno);
            continue;
        }

        dev = malloc(sizeof(*dev));
        memset(dev, 0, sizeof(*dev));
        dev->conn_id = conn->connector_id;

        ret = drm_device_setup(fd, res, conn, dev);
        if (ret) {
            if (ret != -ENOENT) {
                errno = -ret;
                fprintf(stderr, "failed to setup device for connector %u:%u (%d)\n",
                    i, res->connectors[i], errno);
            }
            free(dev);
            drmModeFreeConnector(conn);
            continue;
        }
        drmModeFreeConnector(conn);
        dev->next = drm_devices_list;
        drm_devices_list = dev;
    }

    drmModeFreeResources(res);
    return 0;
}

void save_framebuffer(const char *path, struct drm_device *dev) {
    if (!dev || !dev->map) {
        fprintf(stderr, "Invalid framebuffer data\n");
        return;
    }
    FILE *fp;
    uint32_t headers[13];
    uint32_t image_size = dev->width * dev->height;
    uint32_t file_size = 54 + image_size;

    fp = open(path, "wb");
    if (!fp) {
        fprintf(stderr, "failed to open %s: %s\n", path, strerror(errno));
        return;
    }

    // BMP header
    headers[0] = 0x4D42; // 'BM'
    headers[1] = file_size;
    headers[2] = 0;
    headers[3] = 54; // Offset to image data
    headers[4] = 40; // Info header size
    headers[5] = dev->width;
    headers[6] = dev->height;
    headers[7] = 1 | (32 << 16); // Planes and bpp
    headers[8] = 0; // Compression
    headers[9] = image_size;
    headers[10] = 0x130B; // Horizontal resolution (2835 pixels/meter)
    headers[11] = 0x130B; // Vertical resolution (2835 pixels/meter)
    headers[12] = 0; // Colors used and important colors'

    fwrite(headers, 4, 13, fp);
    for (int y = dev->height - 1; y >= 0; y--) {
        fwrite(dev->map + y * dev->stride, 1, dev->stride, fp);
    }

    fclose(fp);
    fprintf(stderr, "saved framebuffer to %s\n", path);
}

