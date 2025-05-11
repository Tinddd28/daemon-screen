#include <kms.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

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
    *x, *y, *src_x, *src_y, *src_w, *src_h = 0;

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

const connector_crtc_pair *get_connector_pair_by_crtc_id(
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

int kms_get_fb(ds_drm *drm, ds_kms_result *result) {
    int ret = -1;
    result->result = KMS_RESULT_OK;
    result->err_msg[0] = '\0';
    result->num_items = 0;

    connector_to_crtc_map c2crtc_map;
    c2crtc_map.maps = 0;
    map_crtc_to_connector_ids(drm, &c2crtc_map);
    
    drmModePlaneResPtr planes = drmModeGetPlaneResources(drm->drm_fd);
    if (!planes) {
        fprintf(stderr, "failed to get plane resources\n");
        return done(planes, &result, -1);
    }

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
        }

        if (!plane->fb_id) {
            if(drmfb)
                drmModeFreeFB2(drmfb);
            if(plane)
                drmModeFreePlane(plane); 
        }

        drmfb = drmModeGetFB2(drm->drm_fd, plane->fb_id);
        if (!drmfb) {
            snprintf(result->err_msg, sizeof(result->err_msg), "failed to get drm fb with id %u, error: %s\n", plane->fb_id, strerror(errno));
            if(drmfb)
                drmModeFreeFB2(drmfb);
            if(plane)
                drmModeFreePlane(plane);
        }

        if (!drmfb->handles[0]) {
            result->result = KMS_RESULT_FAILED_TO_GET_PLANE;
            snprintf(result->err_msg, sizeof(result->err_msg), "drmfb handle is NULL");
            drm_mode_cleanup_handles(drm->drm_fd, drmfb);        
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

int done(drmModePlaneResPtr planes, ds_kms_result *result, int *ret) {
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