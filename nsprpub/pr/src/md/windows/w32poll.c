/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * This file implements _PR_MD_PR_POLL for Win32.
 */

/* The default value of FD_SETSIZE is 64. */
#define FD_SETSIZE 1024

#include "primpl.h"

#if !defined(_PR_GLOBAL_THREADS_ONLY)

struct select_data_s {
    PRInt32 status;
    PRInt32 error;
    fd_set *rd, *wt, *ex;
    const struct timeval *tv;
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

int _PR_NTFiberSafeSelect(
    int nfds,
    fd_set *readfds,
    fd_set *writefds,
    fd_set *exceptfds,
    const struct timeval *timeout)
{
    PRThread *me = _PR_MD_CURRENT_THREAD();
    int ready;

    if (_PR_IS_NATIVE_THREAD(me)) {
        ready = _MD_SELECT(nfds, readfds, writefds, exceptfds, timeout);
    }
    else
    {
        /*
        ** Creating a new thread on each call!!
        ** I guess web server doesn't use non-block I/O.
        */
        PRThread *selectThread;
        struct select_data_s data;
        data.status = 0;
        data.error = 0;
        data.rd = readfds;
        data.wt = writefds;
        data.ex = exceptfds;
        data.tv = timeout;

        selectThread = PR_CreateThread(
            PR_USER_THREAD, _PR_MD_select_thread, &data,
            PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 0);
        if (selectThread == NULL) return -1;

        PR_JoinThread(selectThread);
        ready = data.status;
        if (ready == SOCKET_ERROR) WSASetLastError(data.error);
    }
    return ready;
}

#endif /* !defined(_PR_GLOBAL_THREADS_ONLY) */

PRInt32 _PR_MD_PR_POLL(PRPollDesc *pds, PRIntn npds, PRIntervalTime timeout)
{
    int ready, err;
    fd_set rd, wt, ex;
    fd_set *rdp, *wtp, *exp;
    int nrd, nwt, nex;
    PRFileDesc *bottom;
    PRPollDesc *pd, *epd;
    PRThread *me = _PR_MD_CURRENT_THREAD();

    struct timeval tv, *tvp = NULL;

    if (_PR_PENDING_INTERRUPT(me))
    {
        me->flags &= ~_PR_INTERRUPT;
        PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
        return -1;
    }

    /*
    ** Is it an empty set? If so, just sleep for the timeout and return
    */
    if (0 == npds)
    {
        PR_Sleep(timeout);
        return 0;
    }

    nrd = nwt = nex = 0;
    FD_ZERO(&rd);
    FD_ZERO(&wt);
    FD_ZERO(&ex);

    ready = 0;
    for (pd = pds, epd = pd + npds; pd < epd; pd++)
    {
        SOCKET osfd;
        PRInt16 in_flags_read = 0, in_flags_write = 0;
        PRInt16 out_flags_read = 0, out_flags_write = 0;

        if ((NULL != pd->fd) && (0 != pd->in_flags))
        {
            if (pd->in_flags & PR_POLL_READ)
            {
                in_flags_read = (pd->fd->methods->poll)(
                    pd->fd, (PRInt16)(pd->in_flags & ~PR_POLL_WRITE),
                    &out_flags_read);
            }
            if (pd->in_flags & PR_POLL_WRITE)
            {
                in_flags_write = (pd->fd->methods->poll)(
                    pd->fd, (PRInt16)(pd->in_flags & ~PR_POLL_READ),
                    &out_flags_write);
            }
            if ((0 != (in_flags_read & out_flags_read))
            || (0 != (in_flags_write & out_flags_write)))
            {
                /* this one's ready right now (buffered input) */
                if (0 == ready)
                {
                    /*
                     * We will have to return without calling the
                     * system poll/select function.  So zero the
                     * out_flags fields of all the poll descriptors
                     * before this one.
                     */
                    PRPollDesc *prev;
                    for (prev = pds; prev < pd; prev++)
                    {
                        prev->out_flags = 0;
                    }
                }
                ready += 1;
                pd->out_flags = out_flags_read | out_flags_write;
            }
            else
            {
                pd->out_flags = 0;  /* pre-condition */
                /* make sure this is an NSPR supported stack */
                bottom = PR_GetIdentitiesLayer(pd->fd, PR_NSPR_IO_LAYER);
                PR_ASSERT(NULL != bottom);  /* what to do about that? */
                if ((NULL != bottom)
                && (_PR_FILEDESC_OPEN == bottom->secret->state))
                {
                    if (0 == ready)
                    {
                        osfd = (SOCKET) bottom->secret->md.osfd;
                        if (in_flags_read & PR_POLL_READ)
                        {
                            pd->out_flags |= _PR_POLL_READ_SYS_READ;
                            FD_SET(osfd, &rd);
                            nrd++;
                        }
                        if (in_flags_read & PR_POLL_WRITE)
                        {
                            pd->out_flags |= _PR_POLL_READ_SYS_WRITE;
                            FD_SET(osfd, &wt);
                            nwt++;
                        }
                        if (in_flags_write & PR_POLL_READ)
                        {
                            pd->out_flags |= _PR_POLL_WRITE_SYS_READ;
                            FD_SET(osfd, &rd);
                            nrd++;
                        }
                        if (in_flags_write & PR_POLL_WRITE)
                        {
                            pd->out_flags |= _PR_POLL_WRITE_SYS_WRITE;
                            FD_SET(osfd, &wt);
                            nwt++;
                        }
                        if (pd->in_flags & PR_POLL_EXCEPT) {
                            FD_SET(osfd, &ex);
                            nex++;
                        }
                    }
                }
                else
                {
                    if (0 == ready)
                    {
                        PRPollDesc *prev;
                        for (prev = pds; prev < pd; prev++)
                        {
                            prev->out_flags = 0;
                        }
                    }
                    ready += 1;  /* this will cause an abrupt return */
                    pd->out_flags = PR_POLL_NVAL;  /* bogii */
                }
            }
        }
        else
        {
            pd->out_flags = 0;
        }
    }

    if (0 != ready) return ready;  /* no need to block */

    if ((nrd > FD_SETSIZE) || (nwt > FD_SETSIZE) || (nex > FD_SETSIZE)) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return -1;
    }

    rdp = (0 == nrd) ? NULL : &rd;
    wtp = (0 == nwt) ? NULL : &wt;
    exp = (0 == nex) ? NULL : &ex;

    if ((NULL == rdp) && (NULL == wtp) && (NULL == exp)) {
        PR_Sleep(timeout);
        return 0;
    }

    if (timeout != PR_INTERVAL_NO_TIMEOUT)
    {
        PRInt32 ticksPerSecond = PR_TicksPerSecond();
        tv.tv_sec = timeout / ticksPerSecond;
        tv.tv_usec = PR_IntervalToMicroseconds( timeout % ticksPerSecond );
        tvp = &tv;
    }

#if defined(_PR_GLOBAL_THREADS_ONLY)
    ready = _MD_SELECT(0, rdp, wtp, exp, tvp);
#else
    ready = _PR_NTFiberSafeSelect(0, rdp, wtp, exp, tvp);
#endif

    /*
    ** Now to unravel the select sets back into the client's poll
    ** descriptor list. Is this possibly an area for pissing away
    ** a few cycles or what?
    */
    if (ready > 0)
    {
        ready = 0;
        for (pd = pds, epd = pd + npds; pd < epd; pd++)
        {
            PRInt16 out_flags = 0;
            if ((NULL != pd->fd) && (0 != pd->in_flags))
            {
                SOCKET osfd;
                bottom = PR_GetIdentitiesLayer(pd->fd, PR_NSPR_IO_LAYER);
                PR_ASSERT(NULL != bottom);

                osfd = (SOCKET) bottom->secret->md.osfd;

                if (FD_ISSET(osfd, &rd))
                {
                    if (pd->out_flags & _PR_POLL_READ_SYS_READ)
                        out_flags |= PR_POLL_READ;
                    if (pd->out_flags & _PR_POLL_WRITE_SYS_READ)
                        out_flags |= PR_POLL_WRITE;
                } 
                if (FD_ISSET(osfd, &wt))
                {
                    if (pd->out_flags & _PR_POLL_READ_SYS_WRITE)
                        out_flags |= PR_POLL_READ;
                    if (pd->out_flags & _PR_POLL_WRITE_SYS_WRITE)
                        out_flags |= PR_POLL_WRITE;
                } 
                if (FD_ISSET(osfd, &ex)) out_flags |= PR_POLL_EXCEPT;
            }
            pd->out_flags = out_flags;
            if (out_flags) ready++;
        }
        PR_ASSERT(ready > 0);
    }
    else if (ready == SOCKET_ERROR)
    {
        err = WSAGetLastError();
        if (err == WSAENOTSOCK)
        {
            /* Find the bad fds */
            int optval;
            int optlen = sizeof(optval);
            ready = 0;
            for (pd = pds, epd = pd + npds; pd < epd; pd++)
            {
                pd->out_flags = 0;
                if ((NULL != pd->fd) && (0 != pd->in_flags))
                {
                    bottom = PR_GetIdentitiesLayer(pd->fd, PR_NSPR_IO_LAYER);
                    if (getsockopt(bottom->secret->md.osfd, SOL_SOCKET,
                        SO_TYPE, (char *) &optval, &optlen) == -1)
                    {
                        PR_ASSERT(WSAGetLastError() == WSAENOTSOCK);
                        if (WSAGetLastError() == WSAENOTSOCK)
                        {
                            pd->out_flags = PR_POLL_NVAL;
                            ready++;
                        }
                    }
                }
            }
            PR_ASSERT(ready > 0);
        }
        else _PR_MD_MAP_SELECT_ERROR(err);
    }

    return ready;
}
