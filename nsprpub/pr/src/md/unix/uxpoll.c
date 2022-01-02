/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(_PR_PTHREADS)

#error "This file should not be compiled"

#else  /* defined(_PR_PTHREADS) */

#include "primpl.h"

#include <sys/time.h>

#include <fcntl.h>
#ifdef _PR_USE_POLL
#include <poll.h>
#endif

#if defined(_PR_USE_POLL)
static PRInt32 NativeThreadPoll(
    PRPollDesc *pds, PRIntn npds, PRIntervalTime timeout)
{
    /*
     * This function is mostly duplicated from ptio.s's PR_Poll().
     */
    PRInt32 ready = 0;
    /*
     * For restarting poll() if it is interrupted by a signal.
     * We use these variables to figure out how much time has
     * elapsed and how much of the timeout still remains.
     */
    PRIntn index, msecs;
    struct pollfd *syspoll = NULL;
    PRIntervalTime start, elapsed, remaining;

    syspoll = (struct pollfd*)PR_MALLOC(npds * sizeof(struct pollfd));
    if (NULL == syspoll)
    {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        return -1;
    }
    for (index = 0; index < npds; ++index)
    {
        PRFileDesc *bottom;
        PRInt16 in_flags_read = 0, in_flags_write = 0;
        PRInt16 out_flags_read = 0, out_flags_write = 0;

        if ((NULL != pds[index].fd) && (0 != pds[index].in_flags))
        {
            if (pds[index].in_flags & PR_POLL_READ)
            {
                in_flags_read = (pds[index].fd->methods->poll)(
                                    pds[index].fd,
                                    pds[index].in_flags & ~PR_POLL_WRITE,
                                    &out_flags_read);
            }
            if (pds[index].in_flags & PR_POLL_WRITE)
            {
                in_flags_write = (pds[index].fd->methods->poll)(
                                     pds[index].fd,
                                     pds[index].in_flags & ~PR_POLL_READ,
                                     &out_flags_write);
            }
            if ((0 != (in_flags_read & out_flags_read))
                || (0 != (in_flags_write & out_flags_write)))
            {
                /* this one is ready right now */
                if (0 == ready)
                {
                    /*
                     * We will return without calling the system
                     * poll function.  So zero the out_flags
                     * fields of all the poll descriptors before
                     * this one.
                     */
                    int i;
                    for (i = 0; i < index; i++)
                    {
                        pds[i].out_flags = 0;
                    }
                }
                ready += 1;
                pds[index].out_flags = out_flags_read | out_flags_write;
            }
            else
            {
                pds[index].out_flags = 0;  /* pre-condition */
                /* now locate the NSPR layer at the bottom of the stack */
                bottom = PR_GetIdentitiesLayer(pds[index].fd, PR_NSPR_IO_LAYER);
                PR_ASSERT(NULL != bottom);  /* what to do about that? */
                if ((NULL != bottom)
                    && (_PR_FILEDESC_OPEN == bottom->secret->state))
                {
                    if (0 == ready)
                    {
                        syspoll[index].fd = bottom->secret->md.osfd;
                        syspoll[index].events = 0;  /* pre-condition */
                        if (in_flags_read & PR_POLL_READ)
                        {
                            pds[index].out_flags |=
                                _PR_POLL_READ_SYS_READ;
                            syspoll[index].events |= POLLIN;
                        }
                        if (in_flags_read & PR_POLL_WRITE)
                        {
                            pds[index].out_flags |=
                                _PR_POLL_READ_SYS_WRITE;
                            syspoll[index].events |= POLLOUT;
                        }
                        if (in_flags_write & PR_POLL_READ)
                        {
                            pds[index].out_flags |=
                                _PR_POLL_WRITE_SYS_READ;
                            syspoll[index].events |= POLLIN;
                        }
                        if (in_flags_write & PR_POLL_WRITE)
                        {
                            pds[index].out_flags |=
                                _PR_POLL_WRITE_SYS_WRITE;
                            syspoll[index].events |= POLLOUT;
                        }
                        if (pds[index].in_flags & PR_POLL_EXCEPT) {
                            syspoll[index].events |= POLLPRI;
                        }
                    }
                }
                else
                {
                    if (0 == ready)
                    {
                        int i;
                        for (i = 0; i < index; i++)
                        {
                            pds[i].out_flags = 0;
                        }
                    }
                    ready += 1;  /* this will cause an abrupt return */
                    pds[index].out_flags = PR_POLL_NVAL;  /* bogii */
                }
            }
        }
        else
        {
            /* make poll() ignore this entry */
            syspoll[index].fd = -1;
            syspoll[index].events = 0;
            pds[index].out_flags = 0;
        }
    }

    if (0 == ready)
    {
        switch (timeout)
        {
            case PR_INTERVAL_NO_WAIT: msecs = 0; break;
            case PR_INTERVAL_NO_TIMEOUT: msecs = -1; break;
            default:
                msecs = PR_IntervalToMilliseconds(timeout);
                start = PR_IntervalNow();
        }

retry:
        ready = _MD_POLL(syspoll, npds, msecs);
        if (-1 == ready)
        {
            PRIntn oserror = errno;

            if (EINTR == oserror)
            {
                if (timeout == PR_INTERVAL_NO_TIMEOUT) {
                    goto retry;
                }
                else if (timeout == PR_INTERVAL_NO_WAIT) {
                    ready = 0;
                }
                else
                {
                    elapsed = (PRIntervalTime)(PR_IntervalNow() - start);
                    if (elapsed > timeout) {
                        ready = 0;    /* timed out */
                    }
                    else
                    {
                        remaining = timeout - elapsed;
                        msecs = PR_IntervalToMilliseconds(remaining);
                        goto retry;
                    }
                }
            }
            else {
                _PR_MD_MAP_POLL_ERROR(oserror);
            }
        }
        else if (ready > 0)
        {
            for (index = 0; index < npds; ++index)
            {
                PRInt16 out_flags = 0;
                if ((NULL != pds[index].fd) && (0 != pds[index].in_flags))
                {
                    if (0 != syspoll[index].revents)
                    {
                        /*
                        ** Set up the out_flags so that it contains the
                        ** bits that the highest layer thinks are nice
                        ** to have. Then the client of that layer will
                        ** call the appropriate I/O function and maybe
                        ** the protocol will make progress.
                        */
                        if (syspoll[index].revents & POLLIN)
                        {
                            if (pds[index].out_flags
                                & _PR_POLL_READ_SYS_READ)
                            {
                                out_flags |= PR_POLL_READ;
                            }
                            if (pds[index].out_flags
                                & _PR_POLL_WRITE_SYS_READ)
                            {
                                out_flags |= PR_POLL_WRITE;
                            }
                        }
                        if (syspoll[index].revents & POLLOUT)
                        {
                            if (pds[index].out_flags
                                & _PR_POLL_READ_SYS_WRITE)
                            {
                                out_flags |= PR_POLL_READ;
                            }
                            if (pds[index].out_flags
                                & _PR_POLL_WRITE_SYS_WRITE)
                            {
                                out_flags |= PR_POLL_WRITE;
                            }
                        }
                        if (syspoll[index].revents & POLLPRI) {
                            out_flags |= PR_POLL_EXCEPT;
                        }
                        if (syspoll[index].revents & POLLERR) {
                            out_flags |= PR_POLL_ERR;
                        }
                        if (syspoll[index].revents & POLLNVAL) {
                            out_flags |= PR_POLL_NVAL;
                        }
                        if (syspoll[index].revents & POLLHUP) {
                            out_flags |= PR_POLL_HUP;
                        }
                    }
                }
                pds[index].out_flags = out_flags;
            }
        }
    }

    PR_DELETE(syspoll);
    return ready;

}  /* NativeThreadPoll */
#endif  /* defined(_PR_USE_POLL) */

#if !defined(_PR_USE_POLL)
static PRInt32 NativeThreadSelect(
    PRPollDesc *pds, PRIntn npds, PRIntervalTime timeout)
{
    /*
     * This code is almost a duplicate of w32poll.c's _PR_MD_PR_POLL().
     */
    fd_set rd, wt, ex;
    PRFileDesc *bottom;
    PRPollDesc *pd, *epd;
    PRInt32 maxfd = -1, ready, err;
    PRIntervalTime remaining, elapsed, start;

    struct timeval tv, *tvp = NULL;

    FD_ZERO(&rd);
    FD_ZERO(&wt);
    FD_ZERO(&ex);

    ready = 0;
    for (pd = pds, epd = pd + npds; pd < epd; pd++)
    {
        PRInt16 in_flags_read = 0, in_flags_write = 0;
        PRInt16 out_flags_read = 0, out_flags_write = 0;

        if ((NULL != pd->fd) && (0 != pd->in_flags))
        {
            if (pd->in_flags & PR_POLL_READ)
            {
                in_flags_read = (pd->fd->methods->poll)(
                                    pd->fd, pd->in_flags & ~PR_POLL_WRITE, &out_flags_read);
            }
            if (pd->in_flags & PR_POLL_WRITE)
            {
                in_flags_write = (pd->fd->methods->poll)(
                                     pd->fd, pd->in_flags & ~PR_POLL_READ, &out_flags_write);
            }
            if ((0 != (in_flags_read & out_flags_read))
                || (0 != (in_flags_write & out_flags_write)))
            {
                /* this one's ready right now */
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
                        PRInt32 osfd = bottom->secret->md.osfd;
                        if (osfd > maxfd) {
                            maxfd = osfd;
                        }
                        if (in_flags_read & PR_POLL_READ)
                        {
                            pd->out_flags |= _PR_POLL_READ_SYS_READ;
                            FD_SET(osfd, &rd);
                        }
                        if (in_flags_read & PR_POLL_WRITE)
                        {
                            pd->out_flags |= _PR_POLL_READ_SYS_WRITE;
                            FD_SET(osfd, &wt);
                        }
                        if (in_flags_write & PR_POLL_READ)
                        {
                            pd->out_flags |= _PR_POLL_WRITE_SYS_READ;
                            FD_SET(osfd, &rd);
                        }
                        if (in_flags_write & PR_POLL_WRITE)
                        {
                            pd->out_flags |= _PR_POLL_WRITE_SYS_WRITE;
                            FD_SET(osfd, &wt);
                        }
                        if (pd->in_flags & PR_POLL_EXCEPT) {
                            FD_SET(osfd, &ex);
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

    if (0 != ready) {
        return ready;    /* no need to block */
    }

    remaining = timeout;
    start = PR_IntervalNow();

retry:
    if (timeout != PR_INTERVAL_NO_TIMEOUT)
    {
        PRInt32 ticksPerSecond = PR_TicksPerSecond();
        tv.tv_sec = remaining / ticksPerSecond;
        tv.tv_usec = PR_IntervalToMicroseconds( remaining % ticksPerSecond );
        tvp = &tv;
    }

    ready = _MD_SELECT(maxfd + 1, &rd, &wt, &ex, tvp);

    if (ready == -1 && errno == EINTR)
    {
        if (timeout == PR_INTERVAL_NO_TIMEOUT) {
            goto retry;
        }
        else
        {
            elapsed = (PRIntervalTime) (PR_IntervalNow() - start);
            if (elapsed > timeout) {
                ready = 0;    /* timed out */
            }
            else
            {
                remaining = timeout - elapsed;
                goto retry;
            }
        }
    }

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
                PRInt32 osfd;
                bottom = PR_GetIdentitiesLayer(pd->fd, PR_NSPR_IO_LAYER);
                PR_ASSERT(NULL != bottom);

                osfd = bottom->secret->md.osfd;

                if (FD_ISSET(osfd, &rd))
                {
                    if (pd->out_flags & _PR_POLL_READ_SYS_READ) {
                        out_flags |= PR_POLL_READ;
                    }
                    if (pd->out_flags & _PR_POLL_WRITE_SYS_READ) {
                        out_flags |= PR_POLL_WRITE;
                    }
                }
                if (FD_ISSET(osfd, &wt))
                {
                    if (pd->out_flags & _PR_POLL_READ_SYS_WRITE) {
                        out_flags |= PR_POLL_READ;
                    }
                    if (pd->out_flags & _PR_POLL_WRITE_SYS_WRITE) {
                        out_flags |= PR_POLL_WRITE;
                    }
                }
                if (FD_ISSET(osfd, &ex)) {
                    out_flags |= PR_POLL_EXCEPT;
                }
            }
            pd->out_flags = out_flags;
            if (out_flags) {
                ready++;
            }
        }
        PR_ASSERT(ready > 0);
    }
    else if (ready < 0)
    {
        err = _MD_ERRNO();
        if (err == EBADF)
        {
            /* Find the bad fds */
            ready = 0;
            for (pd = pds, epd = pd + npds; pd < epd; pd++)
            {
                pd->out_flags = 0;
                if ((NULL != pd->fd) && (0 != pd->in_flags))
                {
                    bottom = PR_GetIdentitiesLayer(pd->fd, PR_NSPR_IO_LAYER);
                    if (fcntl(bottom->secret->md.osfd, F_GETFL, 0) == -1)
                    {
                        pd->out_flags = PR_POLL_NVAL;
                        ready++;
                    }
                }
            }
            PR_ASSERT(ready > 0);
        }
        else {
            _PR_MD_MAP_SELECT_ERROR(err);
        }
    }

    return ready;
}  /* NativeThreadSelect */
#endif  /* !defined(_PR_USE_POLL) */

static PRInt32 LocalThreads(
    PRPollDesc *pds, PRIntn npds, PRIntervalTime timeout)
{
    PRPollDesc *pd, *epd;
    PRInt32 ready, pdcnt;
    _PRUnixPollDesc *unixpds, *unixpd;

    /*
     * XXX
     *        PRPollDesc has a PRFileDesc field, fd, while the IOQ
     *        is a list of PRPollQueue structures, each of which contains
     *        a _PRUnixPollDesc. A _PRUnixPollDesc struct contains
     *        the OS file descriptor, osfd, and not a PRFileDesc.
     *        So, we have allocate memory for _PRUnixPollDesc structures,
     *        copy the flags information from the pds list and have pq
     *        point to this list of _PRUnixPollDesc structures.
     *
     *        It would be better if the memory allocation can be avoided.
     */

    unixpd = unixpds = (_PRUnixPollDesc*)
                       PR_MALLOC(npds * sizeof(_PRUnixPollDesc));
    if (NULL == unixpds)
    {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        return -1;
    }

    ready = 0;
    for (pdcnt = 0, pd = pds, epd = pd + npds; pd < epd; pd++)
    {
        PRFileDesc *bottom;
        PRInt16 in_flags_read = 0, in_flags_write = 0;
        PRInt16 out_flags_read = 0, out_flags_write = 0;

        if ((NULL != pd->fd) && (0 != pd->in_flags))
        {
            if (pd->in_flags & PR_POLL_READ)
            {
                in_flags_read = (pd->fd->methods->poll)(
                                    pd->fd, pd->in_flags & ~PR_POLL_WRITE, &out_flags_read);
            }
            if (pd->in_flags & PR_POLL_WRITE)
            {
                in_flags_write = (pd->fd->methods->poll)(
                                     pd->fd, pd->in_flags & ~PR_POLL_READ, &out_flags_write);
            }
            if ((0 != (in_flags_read & out_flags_read))
                || (0 != (in_flags_write & out_flags_write)))
            {
                /* this one's ready right now */
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
                bottom = PR_GetIdentitiesLayer(pd->fd, PR_NSPR_IO_LAYER);
                PR_ASSERT(NULL != bottom);  /* what to do about that? */
                if ((NULL != bottom)
                    && (_PR_FILEDESC_OPEN == bottom->secret->state))
                {
                    if (0 == ready)
                    {
                        unixpd->osfd = bottom->secret->md.osfd;
                        unixpd->in_flags = 0;
                        if (in_flags_read & PR_POLL_READ)
                        {
                            unixpd->in_flags |= _PR_UNIX_POLL_READ;
                            pd->out_flags |= _PR_POLL_READ_SYS_READ;
                        }
                        if (in_flags_read & PR_POLL_WRITE)
                        {
                            unixpd->in_flags |= _PR_UNIX_POLL_WRITE;
                            pd->out_flags |= _PR_POLL_READ_SYS_WRITE;
                        }
                        if (in_flags_write & PR_POLL_READ)
                        {
                            unixpd->in_flags |= _PR_UNIX_POLL_READ;
                            pd->out_flags |= _PR_POLL_WRITE_SYS_READ;
                        }
                        if (in_flags_write & PR_POLL_WRITE)
                        {
                            unixpd->in_flags |= _PR_UNIX_POLL_WRITE;
                            pd->out_flags |= _PR_POLL_WRITE_SYS_WRITE;
                        }
                        if ((in_flags_read | in_flags_write) & PR_POLL_EXCEPT)
                        {
                            unixpd->in_flags |= _PR_UNIX_POLL_EXCEPT;
                        }
                        unixpd++; pdcnt++;
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
    }

    if (0 != ready)
    {
        /* no need to block */
        PR_DELETE(unixpds);
        return ready;
    }

    ready = _PR_WaitForMultipleFDs(unixpds, pdcnt, timeout);

    /*
     * Copy the out_flags from the _PRUnixPollDesc structures to the
     * user's PRPollDesc structures and free the allocated memory
     */
    unixpd = unixpds;
    for (pd = pds, epd = pd + npds; pd < epd; pd++)
    {
        PRInt16 out_flags = 0;
        if ((NULL != pd->fd) && (0 != pd->in_flags))
        {
            /*
             * take errors from the poll operation,
             * the R/W bits from the request
             */
            if (0 != unixpd->out_flags)
            {
                if (unixpd->out_flags & _PR_UNIX_POLL_READ)
                {
                    if (pd->out_flags & _PR_POLL_READ_SYS_READ) {
                        out_flags |= PR_POLL_READ;
                    }
                    if (pd->out_flags & _PR_POLL_WRITE_SYS_READ) {
                        out_flags |= PR_POLL_WRITE;
                    }
                }
                if (unixpd->out_flags & _PR_UNIX_POLL_WRITE)
                {
                    if (pd->out_flags & _PR_POLL_READ_SYS_WRITE) {
                        out_flags |= PR_POLL_READ;
                    }
                    if (pd->out_flags & _PR_POLL_WRITE_SYS_WRITE) {
                        out_flags |= PR_POLL_WRITE;
                    }
                }
                if (unixpd->out_flags & _PR_UNIX_POLL_EXCEPT) {
                    out_flags |= PR_POLL_EXCEPT;
                }
                if (unixpd->out_flags & _PR_UNIX_POLL_ERR) {
                    out_flags |= PR_POLL_ERR;
                }
                if (unixpd->out_flags & _PR_UNIX_POLL_NVAL) {
                    out_flags |= PR_POLL_NVAL;
                }
                if (unixpd->out_flags & _PR_UNIX_POLL_HUP) {
                    out_flags |= PR_POLL_HUP;
                }
            }
            unixpd++;
        }
        pd->out_flags = out_flags;
    }

    PR_DELETE(unixpds);

    return ready;
}  /* LocalThreads */

#if defined(_PR_USE_POLL)
#define NativeThreads NativeThreadPoll
#else
#define NativeThreads NativeThreadSelect
#endif

PRInt32 _MD_pr_poll(PRPollDesc *pds, PRIntn npds, PRIntervalTime timeout)
{
    PRInt32 rv = 0;
    PRThread *me = _PR_MD_CURRENT_THREAD();

    if (_PR_PENDING_INTERRUPT(me))
    {
        me->flags &= ~_PR_INTERRUPT;
        PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
        return -1;
    }
    if (0 == npds) {
        PR_Sleep(timeout);
    }
    else if (_PR_IS_NATIVE_THREAD(me)) {
        rv = NativeThreads(pds, npds, timeout);
    }
    else {
        rv = LocalThreads(pds, npds, timeout);
    }

    return rv;
}  /* _MD_pr_poll */

#endif  /* defined(_PR_PTHREADS) */

/* uxpoll.c */

