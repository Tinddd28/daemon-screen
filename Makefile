# build:
# 	gcc include/util.h src/util.c src/drmcheck.c -o drm_test -ldrm -I/usr/include/libdrm    

CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -I/usr/include/libdrm
LDFLAGS = -ldrm

TARGET = drm_test

SRCS = src/main.c src/mydrm.c src/usage.c

OBJS = $(SRCS:.c=.o)

DEPS = include/mydrm.h include/usage.h


all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

# b2:
# 	gcc include/mydrm.h include/usage.h src/mydrm.c src/usage.c src/main.c -ldrm -I/usr/include/libdrm

.PHONY: all clean