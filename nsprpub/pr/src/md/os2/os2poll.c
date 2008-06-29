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
 * This file implements _PR_MD_PR_POLL for OS/2.
 */

#ifdef XP_OS2_EMX
 #include <sys/time.h> /* For timeval. */
#endif

#include "primpl.h"

#ifndef BSD_SELECT
/* Utility functions called when using OS/2 select */

PRBool IsSocketSet( PRInt32 osfd, int* socks, int start, int count )
{
  int i;
  PRBool isSet = PR_FALSE;

  for( i = start; i < start+count; i++ )
  {
    if( socks[i] == osfd )
      isSet = PR_TRUE;
  }
  
  return isSet; 
}
#endif

PRInt32 _PR_MD_PR_POLL(PRPollDesc *pds, PRIntn npds, PRIntervalTime timeout)
{
#ifdef BSD_SELECT
    fd_set rd, wt, ex;
#else
    int rd, wt, ex;
    int* socks;
    unsigned long msecs;
    int i, j;
#endif
    PRFileDesc *bottom;
    PRPollDesc *pd, *epd;
    PRInt32 maxfd = -1, ready, err;
    PRIntervalTime remaining, elapsed, start;

#ifdef BSD_SELECT
    struct timeval tv, *tvp = NULL;

    FD_ZERO(&rd);
    FD_ZERO(&wt);
    FD_ZERO(&ex);
#else
    rd = 0;
    wt = 0;
    ex = 0;
    socks = (int) PR_MALLOC( npds * 3 * sizeof(int) );
    
    if (!socks)
    {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        return -1;
    }
#endif

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
            if ((0 != (in_flags_read & out_flags_read)) ||
                (0 != (in_flags_write & out_flags_write)))
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
                if ((NULL != bottom) &&
                    (_PR_FILEDESC_OPEN == bottom->secret->state))
                {
                    if (0 == ready)
                    {
                        PRInt32 osfd = bottom->secret->md.osfd;
                        if (osfd > maxfd) 
                            maxfd = osfd;
                        if (in_flags_read & PR_POLL_READ)
                        {
                            pd->out_flags |= _PR_POLL_READ_SYS_READ;
#ifdef BSD_SELECT
                            FD_SET(osfd, &rd);
#else
                            socks[rd] = osfd;
                            rd++;              
#endif
                        }
                        if (in_flags_read & PR_POLL_WRITE)
                        {
                            pd->out_flags |= _PR_POLL_READ_SYS_WRITE;
#ifdef BSD_SELECT
                            FD_SET(osfd, &wt);
#else
                            socks[npds+wt] = osfd;
                            wt++;              
#endif
                        }
                        if (in_flags_write & PR_POLL_READ)
                        {
                            pd->out_flags |= _PR_POLL_WRITE_SYS_READ;
#ifdef BSD_SELECT
                            FD_SET(osfd, &rd);
#else
                            socks[rd] = osfd;
                            rd++;              
#endif
                        }
                        if (in_flags_write & PR_POLL_WRITE)
                        {
                            pd->out_flags |= _PR_POLL_WRITE_SYS_WRITE;
#ifdef BSD_SELECT
                            FD_SET(osfd, &wt);
#else
                            socks[npds+wt] = osfd;
                            wt++;              
#endif
                        }
                        if (pd->in_flags & PR_POLL_EXCEPT)
                        {
#ifdef BSD_SELECT
                            FD_SET(osfd, &ex);
#else
                            socks[npds*2+ex] = osfd;
                            ex++;
#endif
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

    if (0 != ready)
    {
#ifndef BSD_SELECT
        PR_Free(socks);
#endif
        return ready;  /* no need to block */
    }

    remaining = timeout;
    start = PR_IntervalNow();

retry:
#ifdef BSD_SELECT
    if (timeout != PR_INTERVAL_NO_TIMEOUT)
    {
        PRInt32 ticksPerSecond = PR_TicksPerSecond();
        tv.tv_sec = remaining / ticksPerSecond;
        tv.tv_usec = PR_IntervalToMicroseconds( remaining % ticksPerSecond );
        tvp = &tv;
    }

    ready = bsdselect(maxfd + 1, &rd, &wt, &ex, tvp);
#else
    switch (timeout)
    {
        case PR_INTERVAL_NO_WAIT:
            msecs = 0;
            break;
        case PR_INTERVAL_NO_TIMEOUT:
            msecs = -1;
            break;
        default:
            msecs = PR_IntervalToMilliseconds(remaining);
    }

     /* compact array */
    for( i = rd, j = npds; j < npds+wt; i++,j++ )
        socks[i] = socks[j];
    for( i = rd+wt, j = npds*2; j < npds*2+ex; i++,j++ )
        socks[i] = socks[j];
    
    ready = os2_select(socks, rd, wt, ex, msecs);
#endif

    if (ready == -1 && errno == EINTR)
    {
        if (timeout == PR_INTERVAL_NO_TIMEOUT)
            goto retry;
        else
        {
            elapsed = (PRIntervalTime) (PR_IntervalNow() - start);
            if (elapsed > timeout)
                ready = 0;  /* timed out */
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

#ifdef BSD_SELECT
                if (FD_ISSET(osfd, &rd))
#else
                if( IsSocketSet(osfd, socks, 0, rd) )        
#endif
                {
                    if (pd->out_flags & _PR_POLL_READ_SYS_READ)
                        out_flags |= PR_POLL_READ;
                    if (pd->out_flags & _PR_POLL_WRITE_SYS_READ)
                        out_flags |= PR_POLL_WRITE;
                } 

#ifdef BSD_SELECT
                if (FD_ISSET(osfd, &wt))
#else
                if( IsSocketSet(osfd, socks, rd, wt) )        
#endif
                {
                    if (pd->out_flags & _PR_POLL_READ_SYS_WRITE)
                        out_flags |= PR_POLL_READ;
                    if (pd->out_flags & _PR_POLL_WRITE_SYS_WRITE)
                        out_flags |= PR_POLL_WRITE;
                } 

#ifdef BSD_SELECT
                if (FD_ISSET(osfd, &ex))
#else
                if( IsSocketSet(osfd, socks, rd+wt, ex) )        
#endif
                {
                    out_flags |= PR_POLL_EXCEPT;
                }
            }
            pd->out_flags = out_flags;
            if (out_flags) ready++;
        }
        PR_ASSERT(ready > 0);
    }
    else if (ready < 0)
    {
        err = _MD_ERRNO();
        if (err == EBADF)
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
                        PR_ASSERT(sock_errno() == ENOTSOCK);
                        if (sock_errno() == ENOTSOCK)
                        {
                            pd->out_flags = PR_POLL_NVAL;
                            ready++;
                        }
                    }
                }
            }
            PR_ASSERT(ready > 0);
        }
        else
            _PR_MD_MAP_SELECT_ERROR(err);
    }

#ifndef BSD_SELECT
    PR_Free(socks);
#endif
    return ready;
}

