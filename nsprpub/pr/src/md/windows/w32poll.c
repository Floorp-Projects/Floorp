/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
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
 * This file implements _PR_MD_PR_POLL for Win32.
 */

#include "primpl.h"

#if !defined(_PR_GLOBAL_THREADS_ONLY)

struct select_data_s {
    PRInt32 status;
    PRInt32 error;
    fd_set *rd, *wt, *ex;
    struct timeval *tv;
};

static void
_PR_MD_select_thread(void *cdata)
{
    struct select_data_s *cd = (struct select_data_s *)cdata;

    cd->status = select(0, cd->rd, cd->wt, cd->ex, cd->tv);

    if (cd->status == SOCKET_ERROR) {
        cd->error = WSAGetLastError();
    }
}

#endif /* !defined(_PR_GLOBAL_THREADS_ONLY) */

PRInt32
_PR_MD_PR_POLL(PRPollDesc *pds, PRIntn npds, PRIntervalTime timeout)
{
    PRPollDesc *pd, *epd;
    int n, err;
    PRThread *me = _PR_MD_CURRENT_THREAD();

    fd_set rd, wt, ex;
    struct timeval tv, *tvp = NULL;

    FD_ZERO(&rd);
    FD_ZERO(&wt);
    FD_ZERO(&ex);

    for (pd = pds, epd = pd + npds; pd < epd; pd++) {
        SOCKET osfd;
        PRInt16 in_flags = pd->in_flags;
        PRFileDesc *bottom = pd->fd;

        if ((NULL == bottom) || (in_flags == 0)) {
            continue;
        }
        while (bottom->lower != NULL) {
            bottom = bottom->lower;
        }
        osfd = (SOCKET) bottom->secret->md.osfd;

        if (in_flags & PR_POLL_READ)  {
            FD_SET(osfd, &rd);
        }
        if (in_flags & PR_POLL_WRITE) {
            FD_SET(osfd, &wt);
        }
        if (in_flags & PR_POLL_EXCEPT) {
            FD_SET(osfd, &ex);
        }
    }
    if (timeout != PR_INTERVAL_NO_TIMEOUT) {
        tv.tv_sec = PR_IntervalToSeconds(timeout);
        tv.tv_usec = PR_IntervalToMicroseconds(timeout) % PR_USEC_PER_SEC;
        tvp = &tv;
    }

#if defined(_PR_GLOBAL_THREADS_ONLY)
    n = _MD_SELECT(0, &rd, &wt, &ex, tvp);
#else
    if (_PR_IS_NATIVE_THREAD(me)) {
        n = _MD_SELECT(0, &rd, &wt, &ex, tvp);
    } else {
        PRThread *selectThread;
        struct select_data_s data;
        data.status = 0;
        data.error = 0;
        data.rd = &rd;
        data.wt = &wt;
        data.ex = &ex;
        data.tv = tvp;

        selectThread = PR_CreateThread(PR_USER_THREAD,
                       _PR_MD_select_thread,
                       &data,
                       PR_PRIORITY_NORMAL,
                       PR_GLOBAL_THREAD,
                       PR_JOINABLE_THREAD,
                       0);
        if (selectThread == NULL) {
            return -1;
        }
        PR_JoinThread(selectThread);
        n = data.status;
        if (n == SOCKET_ERROR) {
            WSASetLastError(data.error);
        }
    }
#endif

    if (n > 0) {
        n = 0;
        for (pd = pds, epd = pd + npds; pd < epd; pd++) {
            SOCKET osfd;
            PRInt16 in_flags = pd->in_flags;
            PRInt16 out_flags = 0;
            PRFileDesc *bottom = pd->fd;

            if ((NULL == bottom) || (in_flags == 0)) {
                pd->out_flags = 0;
                continue;
            }
            while (bottom->lower != NULL) {
                bottom = bottom->lower;
            }
            osfd = (SOCKET) bottom->secret->md.osfd;

            if ((in_flags & PR_POLL_READ) && FD_ISSET(osfd, &rd))  {
                out_flags |= PR_POLL_READ;
            }
            if ((in_flags & PR_POLL_WRITE) && FD_ISSET(osfd, &wt)) {
                out_flags |= PR_POLL_WRITE;
            }
            if ((in_flags & PR_POLL_EXCEPT) && FD_ISSET(osfd, &ex)) {
                out_flags |= PR_POLL_EXCEPT;
            }
            pd->out_flags = out_flags;
            if (out_flags) {
                n++;
            }
        }
        PR_ASSERT(n > 0);
    } else if (n == SOCKET_ERROR) {
        err = WSAGetLastError();
        if (err == WSAENOTSOCK) {
            /* Find the bad fds */
            n = 0;
            for (pd = pds, epd = pd + npds; pd < epd; pd++) {
                int optval;
                int optlen = sizeof(optval);
                PRFileDesc *bottom = pd->fd;

                pd->out_flags = 0;
                if ((NULL == bottom) || (pd->in_flags == 0)) {
                    continue;
                }
                while (bottom->lower != NULL) {
                    bottom = bottom->lower;
                }
                if (getsockopt(bottom->secret->md.osfd, SOL_SOCKET,
                        SO_TYPE, (char *) &optval, &optlen) == -1) {
                    PR_ASSERT(WSAGetLastError() == WSAENOTSOCK);
                    if (WSAGetLastError() == WSAENOTSOCK) {
                        pd->out_flags = PR_POLL_NVAL;
                        n++;
                    }
                }
            }
            PR_ASSERT(n > 0);
        } else {
            _PR_MD_MAP_SELECT_ERROR(err);
        }
    }

    return n;
}
