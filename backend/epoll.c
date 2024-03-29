/*
 * MPLX3
 * Copyright © 2011 Felipe Astroza
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <mplx3.h>
#include <sys/types.h>
#define _GNU_SOURCE
#include <sys/socket.h>
#include <errno.h>
extern int errno;

/*
 * Edge-Triggered (ET) VS Level-Triggered (LT)
 *
 * Debido a que todos los hilos comparten la misma interfaz epoll, LT provocaria que el mismo evento
 * se repitiese en otros hilos, es por eso que uso ET.
 */

void uqueue_init(uqueue *q)
{
	q->epoll_iface = epoll_create(EPOLL_QUEUE_SIZE);
}

void uqueue_event_init(mplx3_event *ev)
{
	ev->data.events_count = 0;
	ev->data.current_event = 0;
}

int uqueue_wait(uqueue *q, mplx3_event *ev, int timeout)
{
	struct epoll_event *ee;
	int wait_ret;

	ev->data.current_event++;

	if(ev->data.events_count <= ev->data.current_event) {
		do
			wait_ret = epoll_wait(q->epoll_iface, ev->data.events, EVENTS, timeout*1000);
		while(wait_ret == -1 && errno == EINTR);
		
		if(wait_ret == 0)
			return 0;

		ev->data.events_count = wait_ret;
		ev->data.current_event = 0;
	}

	ee = ev->data.events + ev->data.current_event;
	if(ee->events & (EPOLLRDHUP|EPOLLERR|EPOLLHUP))
		ev->type = MPLX3_DISCONNECT_EVENT;
	else if(ee->events & EPOLLIN)
		ev->type = MPLX3_RECEIVE_EVENT;
	else if(ee->events & EPOLLOUT)
		ev->type = MPLX3_SEND_EVENT;
	ev->ep = ee->data.ptr;

	return 1;
}

void uqueue_watch(uqueue *q, mplx3_endpoint *ep)
{
	struct epoll_event ev;

	ev.data.ptr = ep;
	ev.events = EPOLLRDHUP|EPOLLET|ep->new_filter;
	epoll_ctl(q->epoll_iface, EPOLL_CTL_ADD, ep->sockfd, &ev); 
}

void uqueue_unwatch(uqueue *q, mplx3_endpoint *ep)
{
	epoll_ctl(q->epoll_iface, EPOLL_CTL_DEL, ep->sockfd, NULL);
}

void uqueue_filter_set(uqueue *q, mplx3_endpoint *ep)
{
	struct epoll_event ev;

	ev.data.ptr = ep;
	ev.events = EPOLLRDHUP|EPOLLET|ep->new_filter;
	epoll_ctl(q->epoll_iface, EPOLL_CTL_MOD, ep->sockfd, &ev);
}

int accept_and_set(int fd, struct sockaddr *sa, unsigned int *sa_size)
{
	return accept4(fd, sa, sa_size, SOCK_NONBLOCK);
}
