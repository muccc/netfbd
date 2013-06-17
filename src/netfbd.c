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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

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
		
		/* Should be inverted in the driver */
		uint8_t bit = 1;
		if (pixel_value > threshold) {
			bit = 0;
		}

		module_byte |= bit;
		
		++module_bit;
		if (module_bit % 8 == 0) {
			*(module->memory + fb_byte_idx) = module_byte;
			module_byte = 0x00;
			fb_byte_idx++;
		}
		module_byte <<= 1;
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

int udp6_make_sock(struct addrinfo **psinfo, char host[], int port) {
	int status, sock;
	struct addrinfo sainfo;
	struct sockaddr_in6 sin6;
	int sin6len;
	const int port_len_max = 16;
	char port_str[port_len_max];

	snprintf(port_str, port_len_max, "%d", port);

	sin6len = sizeof(struct sockaddr_in6);

	sock = socket(PF_INET6, SOCK_DGRAM,0);

	memset(&sin6, 0, sizeof(struct sockaddr_in6));
	sin6.sin6_port = htons(0);
	sin6.sin6_family = AF_INET6;
	sin6.sin6_addr = in6addr_any;

	status = bind(sock, (struct sockaddr *)&sin6, sin6len);

	if(-1 == status)
		perror("bind"), exit(1);

	memset(&sainfo, 0, sizeof(struct addrinfo));
	memset(&sin6, 0, sin6len);

	sainfo.ai_flags = 0;
	sainfo.ai_family = PF_INET6;
	sainfo.ai_socktype = SOCK_DGRAM;
	sainfo.ai_protocol = IPPROTO_UDP;
	status = getaddrinfo(host, port_str, &sainfo, psinfo);

	switch (status) {
		case EAI_FAMILY: printf("family\n");
			break;
		case EAI_SOCKTYPE: printf("stype\n");
			break;
		case EAI_BADFLAGS: printf("flag\n");
			break;
		case EAI_NONAME: printf("noname\n");
			break;
		case EAI_SERVICE: printf("service\n");
			break;
	}
   
	return sock;
}

int udp6_sendto(uint8_t *buffer, size_t len, int sock, struct addrinfo **psinfo) {
	int status = sendto(sock, buffer, len, 0,
						(struct sockaddr *)(*psinfo)->ai_addr,
						sizeof(struct sockaddr_in6));
	return status;
}

void udp6_close(int sock, struct addrinfo **psinfo) {
	freeaddrinfo(*psinfo);
	shutdown(sock, 2);
	close(sock);
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

	int module_width = 40;
	int module_height = 16;
	int module_pixels = module_width*module_height;
	int module_bytes = module_pixels/8;
	uint8_t module_memory[module_bytes];

	int sock;
	struct addrinfo psinfo;
	struct addrinfo *foo = &psinfo;
	struct addrinfo **psinfo_p = &foo;
	sock = udp6_make_sock(psinfo_p, "fe80::222:f9ff:fe01:c65%wlan0", 2323);

	for (;;) {
		memset(&module_memory, 0, module_bytes);

		struct flipdot_module module;
		module.memory = (uint8_t *)module_memory;
		module.x = 0;
		module.y = 0;
		module.w = module_width;
		module.h = module_height;
	
		fb_map_into_module(&info, &module, atoi(argv[1]));

		/* printf("#define fb_width %d\n" */
		/* 	   "#define fb_height %d\n" */
		/* 	   "static char_fb_bits[] = {\n", module_width, module_height); */
		/* for (int module_byte = 0; module_byte < module_bytes; ++module_byte) { */
		/* 	printf("0x%02x,", *(module_memory + module_byte)); */
		/* } */
		/* printf("}\n"); */
		/* fprintf(stderr, "."); */

		udp6_sendto(module_memory, module_bytes, sock, psinfo_p);
		int us = 1000000.0 * 1.0/atoi(argv[2]);
		usleep(us);
	}
	udp6_close(sock, psinfo_p);

    fb_close(&info);

    return 0;
}
