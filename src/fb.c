#define _GNU_SOURCE 1

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdint.h>

#include <linux/fb.h>

struct fb_info {
    int fd;
    size_t length;
    struct fb_fix_screeninfo fix_info;
    struct fb_var_screeninfo var_info;
    unsigned char *memory;
};

struct flipdot_module {
	int x;
	int y;
	int w;
	int h;
	uint8_t *memory;
};

uint8_t rgb_to_y(uint8_t r, uint8_t g, uint8_t b) {
	uint16_t y = 66*r + 129*g + 25*r;
	return (y+128) >> 8; 			/* Divide by 255 and round */
}

void fb_map_into_module(struct fb_info *info, struct flipdot_module *module, uint8_t threshold) {
	int module_bits = module->w * module->h;
	int module_bit = 0;
	uint8_t module_byte = 0;
	int fb_byte_idx = 0;
	
	while (module_bit < module_bits) {
		int fb_width = info->var_info.xres;
		int row = module_bit / module->w;
		int col = module_bit % module->w;

		uint8_t *fb_pixel_addr = info->memory + 4*(row*fb_width + col);
		uint8_t r = *(fb_pixel_addr + 0);
		uint8_t g = *(fb_pixel_addr + 1);
		uint8_t b = *(fb_pixel_addr + 2);

		uint8_t pixel_value = rgb_to_y(r, g, b);
		
		uint8_t bit = 0;
		if (pixel_value > threshold) {
			bit = 1;
		}

		module_byte |= bit;
		module_byte <<= 1;	/* TODO: bork */
		
		/* Write byte into module after first byte is full*/
		if (module_bit > 7 && module_bit % 8 == 0) {
						module_byte >>= 1;	/* Shift except last step */
			*(module->memory + fb_byte_idx) = module_byte;
			fb_byte_idx++;
		}
		/* TODO: don't forget the last byte! */
		
		module_bit++;
	}
}

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


/* Pipe into imagemagick display! It outputs xbm. */
int main(int argc, char *argv[]) {
    struct fb_info info;

	int fb_width = 1024;
	int fb_height = 768;

    int ret = fb_init(&info, "/dev/fb0");
	if (ret != 0) {
		perror("Fb");
		exit(1);
	}

    fb_set_mode(&info, fb_width, fb_height);

	int module_width = 20*4;
	int module_height = 16*4;
	int module_pixels = module_width*module_height;
	int module_bytes = module_pixels/8;
	uint8_t module_memory[module_bytes];
	memset(&module_memory, 0, module_bytes);

	struct flipdot_module module;
	module.memory = (uint8_t *)module_memory;
	module.x = 0;
	module.y = 0;
	module.w = module_width;
	module.h = module_height;
	
	fb_map_into_module(&info, &module, 80);

	printf("#define fb_width %d\n"
		   "#define fb_height %d\n"
		   "static char_fb_bits[] = {\n", module_width, module_height);
	for (int module_byte = 0; module_byte < module_bytes; ++module_byte) {
		printf("0x%02x,", *(module_memory + module_byte));
	}
	printf("}\n");

    fb_close(&info);

    return 0;
}
