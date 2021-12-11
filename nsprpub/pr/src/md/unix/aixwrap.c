/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * File:        aixwrap.c
 * Description:
 *     This file contains a single function, _MD_SELECT(), which simply
 *     invokes the select() function.  This file is used in an ugly
 *     hack to override the system select() function on AIX releases
 *     prior to 4.2.  (On AIX 4.2, we use a different mechanism to
 *     override select().)
 */

#ifndef AIX_RENAME_SELECT
#error aixwrap.c should only be used on AIX 3.2 or 4.1
#else

#include <sys/select.h>
#include <sys/poll.h>

int _MD_SELECT(int width, fd_set *r, fd_set *w, fd_set *e, struct timeval *t)
{
    return select(width, r, w, e, t);
}

int _MD_POLL(void *listptr, unsigned long nfds, long timeout)
{
    return poll(listptr, nfds, timeout);
}

#endif /* AIX_RENAME_SELECT */
