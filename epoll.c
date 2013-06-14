#include <sys/epoll.h>

#include <stdio.h>

int epoll_fb_init(int fd) {
	int ep = epoll_create(1);
	int e = epoll_ctl(ep, EPOLL_CTL_ADD, fd, EPOLLIN);
	if (!e == 0) {
		perror("Epoll error");
		return 1;
	}

	return ep;
}

int epoll_fb_wait(int epfd) {
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.ptr = NULL;
	ev.data.fd = 0;
	ev.data.u32 = 0;
	ev.data.u64 = 0:

	int ret = epoll_wait(epfd, &ev, 1, -1);
	if (ret == -1) {
		perror("Epoll timeout");
		return 1;
	}

	return ret;
}


/* int epfd = epoll_fb_init(fb); */
/* int ev_count = 0; */
/* while ((ev_count = epoll_fb_wait(epfd)) != 0) { */
/* 	read(fb); etc */
/* } */
