#include "../include/kms.h"

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>
#include <sys/mman.h>
#include <unistd.h>

bool connector_get_property_by_name(int drmfd, drmModeConnectorPtr props, 
    const char *name, uint64_t *result) {
        for (int i = 0; i < props->count_props; ++i) {
            drmModePropertyPtr prop = drmModeGetProperty(drmfd, props->props[i]);
            if (prop) {
                if (strcmp(prop->name, name) == 0) {
                    *result = props->prop_values[i];
                    drmModeFreeProperty(prop);
                    return true;
                }
                drmModeFreeProperty(prop);
            }
        }
    return false;
}

uint32_t plane_get_properties(int drmfd, uint32_t plane_id, int *x, int *y, 
    int *src_x, int *src_y, int *src_w, int *src_h) 
{
    *x = 0;
    *y = 0;
    *src_x = 0;
    *src_y = 0;
    *src_w = 0;
    *src_h = 0;

    plane_property_mask mask = 0;
    drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(drmfd, plane_id, DRM_MODE_OBJECT_PLANE);
    if (!props) {
        fprintf(stderr, "failed to get plane properties\n");
        return mask;
    }

    for (uint32_t i = 0; i < props->count_props; ++i) {
        drmModePropertyPtr prop = drmModeGetProperty(drmfd, props->props[i]);
        if (!prop) {
            continue;
        }

        const uint32_t type = prop->flags & (DRM_MODE_PROP_LEGACY_TYPE | DRM_MODE_PROP_EXTENDED_TYPE);

        if((type & DRM_MODE_PROP_SIGNED_RANGE) && strcmp(prop->name, "CRTC_X") == 0) {
            *x = (int)props->prop_values[i];
            mask |= PLANE_PROPERTY_X;
        } 

        else if((type & DRM_MODE_PROP_SIGNED_RANGE) && strcmp(prop->name, "CRTC_Y") == 0) {
            *y = (int)props->prop_values[i];
            mask |= PLANE_PROPERTY_Y;
        } 

        else if((type & DRM_MODE_PROP_RANGE) && strcmp(prop->name, "SRC_X") == 0) {
            *src_x = (int)(props->prop_values[i] >> 16);
            mask |= PLANE_PROPERTY_SRC_X;
        } 

        else if((type & DRM_MODE_PROP_RANGE) && strcmp(prop->name, "SRC_Y") == 0) {
            *src_y = (int)(props->prop_values[i] >> 16);
            mask |= PLANE_PROPERTY_SRC_Y;
        } 

        else if((type & DRM_MODE_PROP_RANGE) && strcmp(prop->name, "SRC_W") == 0) {
            *src_w = (int)(props->prop_values[i] >> 16);
            mask |= PLANE_PROPERTY_SRC_W;
        } 

        else if((type & DRM_MODE_PROP_RANGE) && strcmp(prop->name, "SRC_H") == 0) {
            *src_h = (int)(props->prop_values[i] >> 16);
            mask |= PLANE_PROPERTY_SRC_H;
        } 

        else if((type & DRM_MODE_PROP_ENUM) && strcmp(prop->name, "type") == 0) {
            const uint64_t current_enum_value = props->prop_values[i];
            for(int j = 0; j < prop->count_enums; ++j) {
                if(prop->enums[j].value == current_enum_value && strcmp(prop->enums[j].name, "Primary") == 0) {
                    mask |= PLANE_PROPERTY_IS_PRIMARY;
                    break;
                } else if(prop->enums[j].value == current_enum_value && strcmp(prop->enums[j].name, "Cursor") == 0) {
                    mask |= PLANE_PROPERTY_IS_CURSOR;
                    break;
                }
            }
        }

        drmModeFreeProperty(prop);
    }

    drmModeFreeObjectProperties(props);
    return mask;
}

const connector_crtc_pair *get_connector_pair_by_crtc_id (
    const connector_to_crtc_map *c2crtc_map,
    uint32_t crtc_id
) {

    for (int i = 0; i < c2crtc_map->num_maps; ++i) {
        if (c2crtc_map->maps[i].crtc_id == crtc_id) {
            return &c2crtc_map->maps[i];
        }
    }
    return NULL;
}

void map_crtc_to_connector_ids(ds_drm *drm, connector_to_crtc_map *c2crtc_map) {
    c2crtc_map->num_maps = 0;
    drmModeResPtr res = drmModeGetResources(drm->drm_fd);
    if (!res) {
        fprintf(stderr, "failed to get DRM resources\n");
        return;
    }

    for (int i = 0; i < res->count_connectors; ++i) {
        drmModeConnectorPtr conn = drmModeGetConnectorCurrent(drm->drm_fd, res->connectors[i]);
        if (!conn) {
            fprintf(stderr, "skipping connector %d\n", res->connectors[i]);
            continue;
        }

        uint64_t crtc_id = 0;
        connector_get_property_by_name(drm->drm_fd, conn, "CRTC_ID", &crtc_id);
        if (crtc_id == 0) {
            fprintf(stderr, "skipping connector %d with no CRTC_ID\n", res->connectors[i]);
            drmModeFreeConnector(conn);
            continue;
        }

        uint64_t hdr_ouput_metadata_blob_id = 0;
        connector_get_property_by_name(drm->drm_fd, conn, "HDR_OUTPUT_METADATA", &hdr_ouput_metadata_blob_id);
        if (hdr_ouput_metadata_blob_id == 0) {
            fprintf(stderr, "skipping connector %d with no HDR_OUTPUT_METADATA\n", res->connectors[i]);
            drmModeFreeConnector(conn);
            continue;
        }

        c2crtc_map->maps[c2crtc_map->num_maps].connector_id = res->connectors[i];
        c2crtc_map->maps[c2crtc_map->num_maps].crtc_id = crtc_id;
        c2crtc_map->maps[c2crtc_map->num_maps].hdr_metadata_blob_id = hdr_ouput_metadata_blob_id;

        drmModeFreeConnector(conn);
    }

    drmModeFreeResources(res);
}


static bool get_hdr_metadata(int drm_fd, uint64_t hdr_metadata_blob_id, struct hdr_output_metadata *hdr_metadata) {
    drmModePropertyBlobPtr hdr_metadata_blob = drmModeGetPropertyBlob(drm_fd, hdr_metadata_blob_id);
    if(!hdr_metadata_blob)
        return false;

    if(hdr_metadata_blob->length >= sizeof(struct hdr_output_metadata))
        *hdr_metadata = *(struct hdr_output_metadata*)hdr_metadata_blob->data;

    drmModeFreePropertyBlob(hdr_metadata_blob);
    return true;
}

int drm_prime_handles_to_fds(ds_drm *drm, drmModeFB2Ptr drmfb, int *fb_fds) {
    for (int i = 0; i < DS_KMS_MAX_DMA_BUFS; ++i) {
        if (!drmfb->handles[i]) {
            return i;
        }

        const int ret = drmPrimeHandleToFD(drm->drm_fd, drmfb->handles[i], O_RDONLY, &fb_fds[i]);
        if (ret != 0 || fb_fds[i] < 0) {
            return i;
        }
    }
    return DS_KMS_MAX_DMA_BUFS;
}

int done(drmModePlaneResPtr planes, ds_kms_result *result, int ret) {
    if (planes) {
        drmModeFreePlaneResources(planes);
    }
    if (result->num_items > 0) 
        result->result = KMS_RESULT_OK;
    if (result->result == KMS_RESULT_OK) {
        ret = 0;
    } else {
        for (int i = 0; i < result->num_items; ++i) {
            for (int j = 0; j < result->items[i].num_dma_bufs; ++j) {
                ds_kms_dma_buf *dma_buf = &result->items[i].dma_buf[j];
                if (dma_buf->fd > 0) {
                    close(dma_buf->fd);
                    dma_buf->fd = -1;
                }
            }
            result->items[i].num_dma_bufs = 0;
        }
        result->num_items = 0;
    }
    return ret;
}

void drm_mode_cleanup_handles(int drmfd, drmModeFB2Ptr drmfb) {
    for(int i = 0; i < 4; ++i) {
        if(!drmfb->handles[i])
            continue;

        bool already_closed = false;
        for(int j = 0; j < i; ++j) {
            if(drmfb->handles[i] == drmfb->handles[j]) {
                already_closed = true;
                break;
            }
        }

        if(already_closed)
            continue;

        drmCloseBufferHandle(drmfd, drmfb->handles[i]);
    }
}

int kms_get_fb(ds_drm *drm, ds_kms_result *result) {
    int ret = -1;
    result->result = KMS_RESULT_OK;
    result->err_msg[0] = '\0';
    result->num_items = 0;

    connector_to_crtc_map c2crtc_map;
    c2crtc_map.num_maps = 0;
    // printf("before map crtc2c\n");
    map_crtc_to_connector_ids(drm, &c2crtc_map);
    // printf("after map crtc2c\n");
    drmModePlaneResPtr planes = drmModeGetPlaneResources(drm->drm_fd);
    if (!planes) {
        fprintf(stderr, "failed to get plane resources\n");
        return done(planes, result, -1);
    }
    // printf("planeRes count: %d\n", planes->count_planes);
    for (uint32_t i = 0; i < planes->count_planes; ++i) {
        drmModePlanePtr plane = NULL;
        drmModeFB2Ptr drmfb = NULL;
        plane = drmModeGetPlane(drm->drm_fd, planes->planes[i]);
        if (!plane) {
            snprintf(result->err_msg, sizeof(result->err_msg), "failed to get drm plane with id %u, error: %s\n", planes->planes[i], strerror(errno));
            if(drmfb)
                drmModeFreeFB2(drmfb);
            if(plane)
                drmModeFreePlane(plane);
            continue;
        }


        if (!plane->fb_id || plane->fb_id == 0) {
            if(drmfb)
                drmModeFreeFB2(drmfb);
            if(plane)
                drmModeFreePlane(plane); 
            continue;
        }
        // printf("plane fb_id: %d, #(%d)\n", plane->fb_id, i+1);
        drmfb = drmModeGetFB2(drm->drm_fd, plane->fb_id);
        if (!drmfb) {
            snprintf(result->err_msg, sizeof(result->err_msg), "failed to get drm fb with id %u, error: %s\n", plane->fb_id, strerror(errno));
            if(drmfb)
                drmModeFreeFB2(drmfb);
            if(plane)
                drmModeFreePlane(plane);
            continue;
        }
        if (!drmfb->handles[0]) {
            result->result = KMS_RESULT_FAILED_TO_GET_PLANE;
            snprintf(result->err_msg, sizeof(result->err_msg), "drmfb handle is NULL");
            drm_mode_cleanup_handles(drm->drm_fd, drmfb);        
            continue;
        }
        
        int x = 0, y = 0, src_x = 0, src_y = 0, src_w = 0, src_h = 0;
        plane_property_mask mask = plane_get_properties(drm->drm_fd, plane->plane_id, &x, &y, &src_x, &src_y, &src_w, &src_h);
        if (!(mask & PLANE_PROPERTY_IS_PRIMARY) && !(mask & PLANE_PROPERTY_IS_CURSOR)) 
            continue;

        int fb_fds[DS_KMS_MAX_DMA_BUFS];
        const int num_fb_fds = drm_prime_handles_to_fds(drm, drmfb, fb_fds);
        if (num_fb_fds == 0) {
            result->result = KMS_RESULT_FAILED_TO_GET_PLANE;
            snprintf(result->err_msg, sizeof(result->err_msg), "failed to get fd from drm handle, error: %s", strerror(errno));
            drm_mode_cleanup_handles(drm->drm_fd, drmfb);
            continue;
        }

        const int item_index = result->num_items;

        const connector_crtc_pair *crtc_pair = get_connector_pair_by_crtc_id(&c2crtc_map, plane->crtc_id);
        if (crtc_pair && crtc_pair->hdr_metadata_blob_id) {
            result->items[item_index].has_hdr_metadata = get_hdr_metadata(drm->drm_fd, crtc_pair->hdr_metadata_blob_id, &result->items[item_index].hdr_metadata);
        } else {
            result->items[item_index].has_hdr_metadata = false;
        }

        for (int j = 0; j < num_fb_fds; ++j) {
            result->items[item_index].dma_buf[j].fd = fb_fds[j];
            result->items[item_index].dma_buf[j].pitch = drmfb->pitches[j];
            result->items[item_index].dma_buf[j].offset = drmfb->offsets[j];
        }
        result->items[item_index].num_dma_bufs = num_fb_fds;

        result->items[item_index].width = drmfb->width;
        result->items[item_index].height = drmfb->height;
        result->items[item_index].pixel_format = drmfb->pixel_format;
        result->items[item_index].modifier = drmfb->flags & DRM_MODE_FB_MODIFIERS ? drmfb->modifier : DRM_FORMAT_MOD_INVALID;
        result->items[item_index].connector_id = crtc_pair ? crtc_pair->connector_id : 0;
        result->items[item_index].is_cursor = mask & PLANE_PROPERTY_IS_CURSOR;
        if (mask & PLANE_PROPERTY_IS_CURSOR) {
            result->items[item_index].x = x;
            result->items[item_index].y = y;
            result->items[item_index].src_w = 0;
            result->items[item_index].src_h = 0;
        } else {
            result->items[item_index].x = src_x;
            result->items[item_index].y = src_y;
            result->items[item_index].src_w = src_w;
            result->items[item_index].src_h = src_h;
        }

        ++result->num_items;
    }
    return ret;
}

int open_drm_device(const char *card, ds_drm *drm) {
    int res = 0;

    drm->drm_fd = open(card, O_RDONLY);
    if (drm->drm_fd < 0) {
        fprintf(stderr, "failed to open %s, error: %s", card, strerror(errno));
        res = 2;
        close(drm->drm_fd);
        return res;
    }
    // printf("drm device opened\n");
    if (drmSetClientCap(drm->drm_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) != 0) {
        fprintf(stderr, "drmSetClientCap: DRM_CLIENT_CAP_UNIVERSAL_PLANES failed, error: %s\n", strerror(errno));
        res = 2;
        close(drm->drm_fd);
        return res;
    }

    if (drmSetClientCap(drm->drm_fd, DRM_CLIENT_CAP_ATOMIC, 1) != 0){
        fprintf(stderr, "drmSetClientCap DRM_CLIENT_CAP_ATOMIC failed, error: %s", strerror(errno));
    }
    return res;
}

void *map_dma_buf(int fd, size_t size, uint32_t offset) {
    size_t page_size = sysconf(_SC_PAGESIZE);
    size_t aligned_offset = (offset / page_size) * page_size;
    size_t extra_offset = offset - aligned_offset;

    void *data = mmap(NULL, page_size + extra_offset, PROT_READ, MAP_SHARED, fd, aligned_offset);
    if (data == MAP_FAILED) {
        perror("mmap failed");
        return NULL;
    }
    return (uint8_t *)data + extra_offset;
}

void unmap_dma_buf(void *addr, size_t size, size_t offset) {
    size_t page_size = sysconf(_SC_PAGESIZE);
    size_t aligned_offset = (offset / page_size) * page_size;
    size_t extra_offset = offset - aligned_offset;

    munmap((uint8_t *)addr - extra_offset, size + extra_offset);
}

void get_map(ds_kms_result *result) {
    printf("start get map\n");
    for (int i = 0; i < result->num_items; ++i) {
        ds_kms_item *item = &result->items[i];
        printf("Processing item ( #%d ):\n", i+1);
        printf("\tResolution: %ux%u\n", item->width, item->height);
        printf("\tPixelFormat (0x%X)\n", item->pixel_format);
        printf("\tModifier: %lu\n", item->modifier);
        printf("\tConnectorId: %u\n", item->connector_id);
        printf("\tIsCursor: %s\n", item->is_cursor ? "true" : "false");
        printf("\tHasHdrMetadata: %s\n", item->has_hdr_metadata ? "true" : "false");
        for (int j = 0; j < item->num_dma_bufs; ++j) {
            ds_kms_dma_buf *dma_buf = &item->dma_buf[j];
            printf("\t\tDmaBuf ( #%d )\n", j+1);
            printf("\t\t\tFD: %d\n", dma_buf->fd);
            printf("\t\t\tPitch: %u\n", dma_buf->pitch);
            printf("\t\t\tOffset: %d\n", dma_buf->offset);
            size_t size = item->height * dma_buf->pitch;
            void *data = map_dma_buf(dma_buf->fd, size, dma_buf->offset);
            if (!data) {
                fprintf(stderr, "failed to map DMA-BUF for item ( #%d ), buffer ( #%d )", i+1, j);
                continue;
            }

            
            
            uint8_t *pixel_data = (uint8_t *)data;
            uint8_t r = pixel_data[2];
            uint8_t g = pixel_data[1];
            uint8_t b = pixel_data[0];
            uint8_t a = pixel_data[4];

            printf("\t\t\tFirst Pixel: R=%u; G=%u; B=%u; A=%u\n", r, g, b, a);

            unmap_dma_buf(data, size, dma_buf->offset);
        }
    }
}