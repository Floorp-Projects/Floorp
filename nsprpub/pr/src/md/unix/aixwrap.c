/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * File:		aixwrap.c
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
