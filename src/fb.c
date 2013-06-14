#define _GNU_SOURCE 1

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <linux/fb.h>

struct fb_info {
    int fd;
    size_t length;
    struct fb_fix_screeninfo fix_info;
    struct fb_var_screeninfo var_info;
    unsigned char *memory;
};

int fb_reinit(struct fb_info *info) {
    size_t old_length = info->length;

    if (ioctl(info->fd, FBIOGET_FSCREENINFO, &info->fix_info)) {
        perror("fix_info");
        return 1;
    }

    if (info->fix_info.type != FB_TYPE_PACKED_PIXELS)
        exit(42);

    if (ioctl(info->fd, FBIOGET_VSCREENINFO, &info->var_info)) {
        perror("var_info");
        return 1;
    }

    info->length = info->fix_info.line_length *
        (info->var_info.yres + info->var_info.yoffset);

    if(info->memory == NULL)
        info->memory = mmap(NULL, info->length, PROT_READ, MAP_SHARED, info->fd, 0);
    else
        info->memory = mremap(info->memory, old_length, info->length, MREMAP_MAYMOVE);

    if (info->memory == MAP_FAILED) {
       perror("mmap failed");
       return 1;
    }

    return 0;
}

int fb_init(struct fb_info *info, char *path) {
    if ( (info->fd = open(path, O_RDONLY)) < 0 ) {
        perror("open fbdev");
        return 1;
    }
    info->memory = NULL;

    return fb_reinit(info);
}


int fb_close(struct fb_info *info) {
    return close(info->fd);
}


int fb_set_mode(struct fb_info *info, size_t x, size_t y) {
    info->var_info.xres = x;
    info->var_info.yres = y;

    if(ioctl(info->fd, FBIOPUT_VSCREENINFO, &info->var_info)) {
        perror ("put_var_info");
        return 1;
    }

    return fb_reinit(info);
}


int main(int argc, char *argv[]) {
    struct fb_info info;

    fb_init(&info, "/dev/fb1");

    fb_set_mode(&info, 1024, 768);

    fb_close(&info);

    return 0;
}
