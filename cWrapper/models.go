package cwrapper

// #include <stdlib.h>
// #include <string.h>
// #include "../include/kms.h"
import "C"

import (
	"fmt"
	"log"
	"unsafe"
)

type DsDrm struct {
	DRMFd int
}

type DmaBuf struct {
	FD     int
	Pitch  uint32
	Offset uint32
}

type KmsItem struct {
	DmaBufs        []DmaBuf
	NumBufs        int
	Width          uint32
	Height         uint32
	PixelFormat    uint32
	Modifier       uint64
	ConnectorId    uint32
	IsCursor       bool
	HasHdrMetadata bool
	X              int
	Y              int
	SrcW           int
	SrcH           int
	HdrMetadata    string
}

type KmsResult struct {
	Result   int
	Items    []KmsItem
	ErrMsg   string
	NumItems int
}

// func ConvertKmsResult(cResult *C.ds_kms_result) KmsResult {
// 	items := make([]KmsItem, int(cResult.num_items))
// 	for i := 0; i < int(cResult.NumItems); i++ {
// 		cItem := &cResult.items[i]
// 		dmaBufs := make([]DmaBuf, int(cResult.num_dma_bufs))
// 		for j := 0; j < int(cResult.num_dma_bufs); j++ {
// 			dmaBufs[j] = DmaBuf{
// 				FD:     int(cItem.dma_buf[j].fd),
// 				Pitch:  uint32(cItem.dma_buf[j].pitch),
// 				Offset: uint32(cItem.dma_buf[j].offset),
// 			}
// 		}
// 		items[i] = KmsItem{
// 			DmaBufs:        dmaBufs,
// 			Width:          uint32(cItem.width),
// 			Height:         uint32(cItem.height),
// 			PixelFormat:    uint32(cItem.pixel_format),
// 			Modifier:       uint64(cItem.modifier),
// 			ConnectorId:    uint32(cItem.connector_id),
// 			IsCursor:       uint32(cItem.is_cursor),
// 			HasHdrMetadata: uint32(cItem.has_hdr_metadata),
// 			X:              int(cItem.x),
// 			Y:              int(cItem.y),
// 			SrcW:           int(cItem.SrcW),
// 			SrcH:           int(cItem.SrcH),
// 			HdrMetadata:    C.GoString((*C.char)(unsafe.Pointer(&cItem.hdr_metadata))),
// 		}
// 	}
// 	return KmsResult{
// 		Result:   int(cResult.result),
// 		Items:    items,
// 		ErrMsg:   C.GoString((*C.char)(unsafe.Pointer(&cItem.err_msg))),
// 		NumItems: int(cResult.num_items),
// 	}
// }

func OpenDrmDevice(card string) (*DsDrm, error) {
	cCard := C.CString(card)
	defer C.free(unsafe.Pointer(cCard))

	var cDrm C.ds_drm

	result := C.open_drm_device(cCard, &cDrm)
	if result != 0 {
		return nil, fmt.Errorf("failed to open DRM device: %s", card)
	}

	drm := &DsDrm{
		DRMFd: int(cDrm.drm_fd),
	}
	return drm, nil
}

func GetKmsResult(drm *DsDrm) (*KmsResult, error) {
	var cResult C.ds_kms_result
	C.kms_get_fb((*C.ds_drm)(unsafe.Pointer(drm)), &cResult)

	if int(cResult.result) != 0 {
		return nil, fmt.Errorf("failed to get KMS result: %s", C.GoString((*C.char)(unsafe.Pointer(&cResult.err_msg))))
	}

	items := make([]KmsItem, cResult.num_items)
	for i := 0; i < int(cResult.num_items); i++ {
		cItem := &cResult.items[i]
		dmaBufs := make([]DmaBuf, int(cItem.num_dma_bufs))
		for j := 0; j < int(cItem.num_dma_bufs); j++ {
			dmaBufs[j] = DmaBuf{
				FD:     int(cItem.dma_buf[j].fd),
				Pitch:  uint32(cItem.dma_buf[j].pitch),
				Offset: uint32(cItem.dma_buf[j].offset),
			}
		}
		items[i] = KmsItem{
			DmaBufs:        dmaBufs,
			Width:          uint32(cItem.width),
			Height:         uint32(cItem.height),
			PixelFormat:    uint32(cItem.pixel_format),
			Modifier:       uint64(cItem.modifier),
			ConnectorId:    uint32(cItem.connector_id),
			IsCursor:       bool(cItem.is_cursor),
			HasHdrMetadata: bool(cItem.has_hdr_metadata),
			X:              int(cItem.x),
			Y:              int(cItem.y),
			SrcW:           int(cItem.src_w),
			SrcH:           int(cItem.src_h),
			HdrMetadata:    C.GoString((*C.char)(unsafe.Pointer(&cItem.hdr_metadata))),
		}
	}

	return &KmsResult{
		Result:   int(cResult.result),
		Items:    items,
		ErrMsg:   C.GoString((*C.char)(unsafe.Pointer(&cResult.err_msg))),
		NumItems: int(cResult.num_items),
	}, nil
}

func Res() {
	drm, err := OpenDrmDevice("/dev/dri/card0")
	if err != nil {
		log.Fatal(err)
		panic(err)
	}
	log.Println("DRM device opened successfully")
	log.Println(drm.DRMFd)
}
