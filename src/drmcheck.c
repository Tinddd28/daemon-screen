#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/mman.h> 
#include <xf86drm.h>
#include <xf86drmMode.h>


#define DRM_DEVICE "/dev/dri/card0"

static uint32_t find_crtc(int fd, drmModeRes *res, drmModeConnector *conn, uint32_t *taken_crtc)
{
    // for (int i = 0; i < conn->count_encoders; i++) 
    // {
    //     drmModeEncoder *enc = drmModeGetEncoder(fd, conn->encoders)
    // }
}

int main() 
{
    int fd = open(DRM_DEVICE, O_RDWR | O_NONBLOCK);
    if (!fd) 
    {
        perror("failed to open DRM device!");
        return EXIT_FAILURE;
    } 
    printf("successfully open fd\n");
    drmModeRes *resources = drmModeGetResources(fd);
    if (!resources) 
    {
        perror("failed to get DRM resources!");
        close(fd);
        return EXIT_FAILURE;
    }

    drmModeConnector *connector = NULL;
    for (int i = 0; i < resources->count_connectors; i++) 
    {
        connector = drmModeGetConnector(fd, resources->connectors[i]);
        if (connector && connector->connection == DRM_MODE_CONNECTED && connector->count_modes > 0) 
        {
            // printf("connector type: %d\n", connector->connector_type);
            printf("connector_type: %s", drmModeGetConnectorTypeName(connector->connector_type));
            printf("\n\n======================================================\n");
        }
        
       
    }

    if (!connector) 
    {
        fprintf(stderr, "no active connector found\n");
        drmModeFreeResources(resources);
        return EXIT_FAILURE;
    }

    drmModeEncoder *encoder = NULL;
    for (int i = 0; i < connector->count_encoders; i++)
    {
        encoder = drmModeGetEncoder(fd, connector->encoders[i]);
        if (encoder && encoder->crtc_id != 0) 
        {
            printf("encoder id: %d\n", encoder->encoder_id);
            break;
        }

        if (encoder) 
        {
            drmModeFreeEncoder(encoder);
            encoder = NULL;
        }
    }
    if (!encoder || encoder->crtc_id == 0) 
    {
        fprintf(stderr, "no active encoder with valid CRTC found!\n");
        drmModeFreeConnector(connector);
        drmModeFreeResources(resources);
        close(fd);
        return EXIT_FAILURE;
    }
    
    
    drmModeCrtc *crtc = drmModeGetCrtc(fd, encoder->crtc_id);
    if (!crtc) 
    {
        fprintf(stderr, "failed to get CRTC!\n");
        drmModeFreeConnector(connector);
        drmModeFreeResources(resources);
        close(fd);
        return EXIT_FAILURE;
    }
    printf("screen resolution: %dx%d\n", crtc->width, crtc->height);

}