build:
	gcc src/util.h src/util.c src/drmcheck.c -o drm_test -ldrm -I/usr/include/libdrm    

.PHONY: build