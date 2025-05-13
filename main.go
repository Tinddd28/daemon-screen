package main

import (
	"daemon-screen/cwrap"
	"log"
)

func main() {
	fd, err := cwrap.OpenDrmDevice("/dev/dri/card0")
	if err != nil {
		log.Fatal(err)
		panic(err)
	}

	log.Println("DRM device opened successfully")
	result, err := cwrap.GetKmsResult(fd)
	if err != nil {
		log.Fatal(err)
		panic(err)
	}
	log.Println("KMS result initialized successfully")
	log.Printf("err msg: %s\n", result.ErrMsg)
	log.Printf("num items: %d\n", result.NumItems)
	log.Printf("result: %d\n", result.Result)
	for i, item := range result.Items {
		log.Printf("(# %d )\n", i+1)
		log.Printf("\tResolution: %dx%d\n", item.Width, item.Height)
		log.Printf("\tPixelFormat: %d\n", item.PixelFormat)
		log.Printf("\tModifier: %d\n", item.Modifier)
		log.Printf("\tConnectorId: %d\n", item.ConnectorId)
		log.Printf("\tIsCursor: %t\n", item.IsCursor)
		log.Printf("\tHasHdrMetadata: %t\n", item.HasHdrMetadata)
		log.Printf("\tX: %d\n", item.X)
		log.Printf("\tY: %d\n", item.Y)
		log.Printf("\tSrcW: %d\n", item.SrcW)
		log.Printf("\tSrcH: %d\n", item.SrcH)
		log.Printf("\tHdrMetadata: %s\n", item.HdrMetadata)
		for j, buf := range item.DmaBufs {
			log.Printf("\t\tDmaBuf ( #%d )\n", j+1)
			log.Printf("\t\t\tFD: %d\n", buf.FD)
			log.Printf("\t\t\tPitch: %d\n", buf.Pitch)
			log.Printf("\t\t\tOffset: %d\n", buf.Offset)
		}
	}
}
