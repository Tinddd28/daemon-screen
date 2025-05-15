# 	gcc include/util.h src/util.c src/drmcheck.c -o drm_test -ldrm -I/usr/include/libdrm    

CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -I/usr/include/libdrm -I/usr/include/EGL -I/usr/include/GLES2
LDFLAGS = -ldrm -lEGL -lGLESv2 -lgbm -L/usr/lib/x86_64-linux-gnu/  

TARGET = drm_test

SRCS = src/main.c src/kms.c src/draw.c

OBJS = $(SRCS:.c=.o)

DEPS =  include/kms.h

# go:
# 	CGO_CFLAGS="-I/usr/include/libdrm -I./src" CGO_LDFLAGS="-ldrm" go build -o my_program

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) 


.PHONY: all clean 