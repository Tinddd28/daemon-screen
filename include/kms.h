#ifndef KMS_H
#define KMS_h
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <fcntl.h>
#include <errno.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

#include <sys/mman.h>

#define MAX_CONNECTORS 32
#define DS_KMS_MAX_ITEMS 16
#define DS_KMS_MAX_DMA_BUFS 4

// typedef struct ds_drm ds_drm;
// typedef struct connector_crtc_pair connector_crtc_pair;
// typedef struct connector_to_crtc_map connector_to_crtc_map;

typedef struct ds_kms_dma_buf ds_kms_dma_buf;
typedef struct ds_kms_item ds_kms_item;
typedef struct ds_kms_result ds_kms_result;

typedef enum {
    PLANE_PROPERTY_X = 1 << 0,
    PLANE_PROPERTY_Y = 1 << 1,
    PLANE_PROPERTY_SRC_X = 1 << 2,
    PLANE_PROPERTY_SRC_Y = 1 << 3,
    PLANE_PROPERTY_SRC_W = 1 << 4,
    PLANE_PROPERTY_SRC_H = 1 << 5,
    PLANE_PROPERTY_IS_CURSOR = 1 << 6,
    PLANE_PROPERTY_IS_PRIMARY = 1 << 7,
} plane_property_mask;

typedef struct {
    int drm_fd;
} ds_drm;

typedef struct {
    uint32_t connector_id;
    uint64_t crtc_id;
    uint64_t hdr_metadata_blob_id;
} connector_crtc_pair;

typedef struct {
    connector_crtc_pair maps[MAX_CONNECTORS];
    int num_maps;
} connector_to_crtc_map;

struct ds_kms_dma_buf {
    int fd;
    uint32_t pitch;
    uint32_t offset;
};

typedef enum {
    KMS_RESULT_OK,
    KMS_RESULT_INVALID_REQUEST,
    KMS_RESULT_FAILED_TO_GET_PLANE,
    KMS_RESULT_FAILED_TO_GET_PLANES,
    KMS_RESULT_FAILED_TO_SEND
} return_kms_result;

struct ds_kms_item {
    ds_kms_dma_buf dma_buf[DS_KMS_MAX_DMA_BUFS];
    int num_dma_bufs;
    uint32_t width;
    uint32_t height;
    uint32_t pixel_format;
    uint64_t modifier;
    uint32_t connector_id;
    bool is_cursor;
    bool has_hdr_metadata;
    int x;
    int y;
    int src_w;
    int src_h;
    struct hdr_output_metadata hdr_metadata;
};
struct ds_kms_result{
    int result;
    ds_kms_item items[DS_KMS_MAX_ITEMS];
    char err_msg[128];
    int num_items;
};
void drm_mode_cleanup_handles(int drmfd, drmModeFB2Ptr drmfb);
extern int kms_get_fb(ds_drm *drm, ds_kms_result *result);
void map_crtc_to_connector_ids(ds_drm *drm, connector_to_crtc_map *c2crtc_map);
const connector_crtc_pair *get_connector_pair_by_crtc_id (const connector_to_crtc_map *c2crtc_map,uint32_t crtc_id);
uint32_t plane_get_properties(int drmfd, uint32_t plane_id, int *x, int *y, int *src_x, int *src_y, int *src_w, int *src_h);
bool connector_get_property_by_name(int drmfd, drmModeConnectorPtr props, const char *name, uint64_t *result);
int open_drm_device(const char *card, ds_drm *drm);
void *map_dma_buf(int fd, size_t size, uint32_t offset);
int unmap_dma_buf(void *addr, size_t size);
#endif