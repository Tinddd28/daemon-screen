CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -I/usr/include/libdrm
LDFLAGS = -ldrm

# Extra libs for the EGL/GBM capture pipeline.
EGL_CFLAGS = $(shell pkg-config --cflags epoxy gbm libpng)
EGL_LIBS   = $(shell pkg-config --libs epoxy gbm libpng) -lm

# --- PoC screen capture (run as root): KMS dma-buf -> EGL -> PNG ---
capture: src/kms.c src/egl_capture.c src/capture_main.c
	$(CC) $(CFLAGS) $(EGL_CFLAGS) $^ -o ds-capture $(LDFLAGS) $(EGL_LIBS)

# --- No-root self-test of the EGL import/readback/PNG pipeline ---
selftest: src/egl_capture.c src/selftest.c
	$(CC) $(CFLAGS) $(EGL_CFLAGS) $^ -o ds-selftest $(LDFLAGS) $(EGL_LIBS)

# --- Legacy scanner (kms enumeration only) ---
TARGET = drm_test
SRCS = src/main.c src/mydrm.c src/kms.c
OBJS = $(SRCS:.c=.o)
DEPS = include/mydrm.h include/kms.h

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) ds-capture ds-selftest

.PHONY: all capture selftest clean
