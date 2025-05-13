# 	gcc include/util.h src/util.c src/drmcheck.c -o drm_test -ldrm -I/usr/include/libdrm    

CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -I/usr/include/libdrm
LDFLAGS = -ldrm

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
	rm -f $(OBJS) $(TARGET)


.PHONY: all clean