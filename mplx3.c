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

#include <pthread.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <mplx3.h>
#include <time.h>
#include <unistd.h>

static inline void mplx3_monitor_add(mplx3_monitor *mon, mplx3_endpoint *ep)
{
	DLIST_APPEND(mon, ep);
}

static inline void mplx3_monitor_del(mplx3_monitor *mon, mplx3_endpoint *ep)
{
	DLIST_DEL(mon, ep);
}

static inline void mplx3_monitor_init(mplx3_monitor *mon)
{
	DLIST_INIT(mon);
}

static mplx3_endpoint *mplx3_endpoint_clone(mplx3_settings *settings, mplx3_endpoint *ep)
{
	mplx3_endpoint *new_ep;

	new_ep = settings->ep_alloc(sizeof(mplx3_endpoint));
	if(new_ep)
		memcpy(new_ep, ep, sizeof(mplx3_endpoint));

	return new_ep;
}

static void mplx3_endpoint_init(mplx3_endpoint *ep, unsigned int sa_size, const mplx3_callbacks *cb)
{
	ep->listening = 0;
	ep->sa_size = sa_size;
	ep->new_filter = MPLX3_RECEIVE;
	DLIST_NODE_INIT(ep);
	memcpy(&ep->cb, cb, sizeof(mplx3_callbacks));
}

static void mplx3_event_init(mplx3_event *ev)
{
	ev->ep = NULL;
	uqueue_event_init(ev);
}

static int mplx3_dummy_call1(mplx3_endpoint *ep)
{
	return 0;
}

static const mplx3_callbacks mplx3_dummy_calls = {
	.calls_arr = {mplx3_dummy_call1, mplx3_dummy_call1, mplx3_dummy_call1, mplx3_dummy_call1, mplx3_dummy_call1}
};

static inline void mplx3_do_accept(mplx3_settings *settings, mplx3_multiplexer *mplx, mplx3_endpoint *ep)
{
	mplx3_endpoint local_ep, *new_ep;

	mplx3_endpoint_init(&local_ep, ep->sa_size, &mplx3_dummy_calls);
	local_ep.sockfd = accept_and_set(ep->sockfd, &local_ep.sa, &local_ep.sa_size);

	if(local_ep.sockfd != -1) {
		if(ep->cb.ev_accept(&local_ep) < 0 || !(new_ep = mplx3_endpoint_clone(settings, &local_ep)))
			close(local_ep.sockfd);
		else {
			new_ep->elapsed_time = 0;
			mplx3_monitor_add(&mplx->monitor, new_ep);
			uqueue_watch(&mplx->queue, new_ep);
			new_ep->new_filter = -1;
		}
	}
}

static inline void mplx3_do_timeout(mplx3_multiplexer *mplx)
{
	mplx3_endpoint *ep;

	DLIST_FOREACH(&mplx->monitor AS ep) {
		ep->elapsed_time++;
		if(ep->elapsed_time >= ep->timeout) {
			if(ep->cb.ev_timeout(ep) == -1) {
				mplx3_monitor_del(&mplx->monitor, ep);
				shutdown(ep->sockfd, SHUT_RDWR);
			}
			ep->elapsed_time = 0;
		}
	}

}

static void mplx3_thread_n(mplx3_settings *settings)
{
	mplx3_event ev;
	mplx3_multiplexer mplx;
	unsigned long start_time;

	uqueue_init(&mplx.queue);
	mplx3_monitor_init(&mplx.monitor);

	{
		mplx3_endpoint *ep;
		DLIST_FOREACH(&settings->initial_endpoints AS ep)
			uqueue_watch(&mplx.queue, ep);
	}

	mplx3_event_init(&ev);

	start_time = time(NULL);
	do {
		if(uqueue_wait(&mplx.queue, &ev, settings->timeout_granularity)) {
			if(ev.ep->listening)
				mplx3_do_accept(settings, &mplx, ev.ep);
			else {
				ev.ep->elapsed_time = 0;
				ev.ep->cb.calls_arr[ev.type](ev.ep);

				if(ev.type == MPLX3_DISCONNECT_EVENT) {
					uqueue_unwatch(&mplx.queue, ev.ep);
					close(ev.ep->sockfd);
					settings->ep_free(ev.ep);
				} else if(ev.ep->new_filter != -1) {
					uqueue_filter_set(&mplx.queue, ev.ep);
					ev.ep->new_filter = -1;
				}
			}
		}

                if(time(NULL) - start_time >= settings->timeout_granularity) {
			mplx3_do_timeout(&mplx);
                        start_time = time(NULL);
                }
	} while(1);
}

inline void mplx3_endpoint_filter_set(mplx3_endpoint *ep, int filter)
{
	ep->new_filter = filter;
}

int mplx3_listen(mplx3_settings *settings, const char *addr, unsigned short port, ev_call1 ev_accept, void *data)
{
	struct sockaddr_in sa;
	mplx3_endpoint *ep;
	mplx3_callbacks cb;
	socklen_t sa_size;
	int sockfd;
	int ret;
	int status = 1;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if ( sockfd == -1 )
		return -1;

	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	sa.sin_addr.s_addr = inet_addr(addr);
	sa_size = sizeof(struct sockaddr_in);

	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &status, sizeof(status)) == -1)
		goto error;

	ret = bind(sockfd, (struct sockaddr *)&sa, sa_size);
	if ( ret == -1 )
		goto error;

	if (listen(sockfd, MPLX3_CONF_BACKLOG) == -1)
		goto error;

	ep = settings->ep_alloc(sizeof(mplx3_endpoint));
	if(!ep)
		goto error;

	cb.ev_accept = ev_accept;
	mplx3_endpoint_init(ep, sa_size, &cb);
	ep->listening = 1;
	ep->sockfd = sockfd;
	DLIST_APPEND(&settings->initial_endpoints, ep);

	return 0;
error:
	close(sockfd);
	return -1;
}
/*
int mplx3_connect(const char *addr, unsigned short port, mplx3_callbacks *cb, void *data)
{

}

void mplx3_specialize()
{

}
*/
void mplx3_init(mplx3_settings *settings, alloc_func alloc, free_func free, unsigned int threads, unsigned int timeout_granularity)
{
	settings->ep_alloc = alloc;
	settings->ep_free = free;
	settings->threads = threads;
	settings->timeout_granularity = timeout_granularity;
	DLIST_INIT(&settings->initial_endpoints);
}

void mplx3_launch(mplx3_settings *settings)
{
	{
		unsigned int i;
		pthread_attr_t attr;
		pthread_t unused;

		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

		for(i = 1; i < settings->threads; i++)
			pthread_create(&unused, &attr, (void *(*)(void *))mplx3_thread_n, (void *)settings);
	}
	mplx3_thread_n(settings);
}
