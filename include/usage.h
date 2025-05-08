#pragma once

#include <sys/wait.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

extern unsigned long hyscall(int num, void *a0, void *a1, void *a2, void *a3, void *a4, void *a5);

extern unsigned long sys_open(char *fn, int flags);
extern unsigned long sys_read(unsigned long fd, char *buff, unsigned long size);

extern void mem_set(void *p, char n, size_t size);
// extern void *mem_alloc(int size);

extern int sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
extern int sys_munmap(void *addr, size_t length);
// extern void *malloc(size_t size);
// extern void free(void *addr);

extern int sys_ioctl(unsigned long fd, unsigned long cmd, void *ard);
