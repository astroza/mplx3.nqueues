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

#ifndef DLIST_H
#define DLIST_H

typedef struct _dlist_node {
	struct _dlist_node *next, *prev;
} dlist_node; 

typedef struct {
	dlist_node *head, *tail;
} dlist;

#ifndef NULL
#define NULL ((void *)0)
#endif

#define DLIST_INIT(list)		((dlist *)list)->head = NULL; ((dlist *)list)->tail = NULL
#define DLIST_APPEND(list, node)	dlist_append((dlist *)(list), (dlist_node *)(node))
#define DLIST_DEL(list, node)		dlist_del((dlist *)(list), (dlist_node *)(node))
#define DLIST_NODE_INIT(a)
#define DLIST(name)			dlist name##_list
#define DLIST_NODE(name)		dlist_node name##_node
#define AS ,
#define _FOREACH(list, node)		for(node = (void *)((dlist *)list)->head; node != NULL; node = (void *)((dlist_node *)node)->next)
#define DLIST_FOREACH(exp)		_FOREACH(exp)

void dlist_append(dlist *, dlist_node *);
void dlist_del(dlist *, dlist_node *);

#endif
