#include <syscall.h>
#include <usage.h>

unsigned long sys_open(char *fn, int flags) {
    return syscall(SYS_open, fn, (void *)(long) flags, 0, 0, 0, 0);
}

unsigned long sys_read(unsigned long fd, char *buff, unsigned long size) {
    return syscall(SYS_read, (void *)fd, buff, (void *)size, 0, 0, 0);
}

int sys_ioctl(unsigned long fd, unsigned long cmd, void *arg) {
    return syscall(SYS_ioctl, fd, cmd, arg, 0, 0, 0);
}

int sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    return syscall(SYS_mmap, addr, length, prot, flags, fd, offset);
}

int sys_munmap(void *addr, size_t length) {
    return syscall(SYS_munmap, addr, length, 0, 0, 0, 0);
}

void mem_set(void *p, char n, size_t size) {
    char *b = (char *)p;

    for (int i = 0; i < size; i++) {
        *b++ = n;
    }
}


