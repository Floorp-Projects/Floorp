/* -*- Mode: C; tab-width: 8; c-basic-offset: 8; indent-tabs-mode: t -*- */
/* vim:set softtabstop=8 shiftwidth=8 noet: */
/*-
 * Copyright (C) the Mozilla Foundation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified other than the possible
 *    addition of one or more copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *******************************************************************************/

#ifndef linkedlist_h__
#define linkedlist_h__

#include <stddef.h>

typedef struct LinkedList_s LinkedList;

struct LinkedList_s {
	LinkedList *next;
	LinkedList *prev;
};

/* Convert from LinkedList* to foo*. */
#define LinkedList_Get(e, type, prop) \
  (type*)((char*)(e) - offsetof(type, prop))

/* Insert |e| at the beginning of |l|.  */
void LinkedList_InsertHead(LinkedList *l, LinkedList *e)
{
	e->next = l;
	e->prev = l->prev;
	e->next->prev = e;
	e->prev->next = e;
}

void LinkedList_Remove(LinkedList *e)
{
	e->prev->next = e->next;
	e->next->prev = e->prev;
	e->next = e;
	e->prev = e;
}

bool LinkedList_IsEmpty(LinkedList *e)
{
	return e->next == e;
}

void LinkedList_Init(LinkedList *e)
{
	e->next = e;
	e->prev = e;
}

#endif
