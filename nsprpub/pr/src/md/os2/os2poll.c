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
 * This file implements _PR_MD_PR_POLL for OS/2.
 */

#include "primpl.h"

PRInt32
_PR_MD_PR_POLL(PRPollDesc *pds, PRIntn npds,
						PRIntervalTime timeout)
{
    PRPollDesc *pd, *epd;
    PRInt32 n, err, pdcnt;
    PRThread *me = _PR_MD_CURRENT_THREAD();

  	 fd_set rd, wt, ex;
  	 struct timeval tv, *tvp = NULL;
  	 int maxfd = -1;

    /*
  	  * For restarting _MD_SELECT() if it is interrupted by a signal.
  	  * We use these variables to figure out how much time has elapsed
  	  * and how much of the timeout still remains.
  	  */
  	 PRIntervalTime start, elapsed, remaining;

  	 FD_ZERO(&rd);
  	 FD_ZERO(&wt);
  	 FD_ZERO(&ex);

    for (pd = pds, epd = pd + npds; pd < epd; pd++) {
         PRInt32 osfd;
         PRInt16 in_flags = pd->in_flags;
         PRFileDesc *bottom = pd->fd;

         if ((NULL == bottom) || (in_flags == 0)) {
             continue;
         }
         while (bottom->lower != NULL) {
             bottom = bottom->lower;
         }
         osfd = bottom->secret->md.osfd;

   	if (osfd > maxfd) {
   		maxfd = osfd;
   	}
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
   	start = PR_IntervalNow();
   }

retry:
   n = _MD_SELECT(maxfd + 1, &rd, &wt, &ex, tvp);
   if (n == -1 && errno == EINTR) {
     	if (timeout == PR_INTERVAL_NO_TIMEOUT) {
   	   	goto retry;
     	} else {
     		elapsed = (PRIntervalTime) (PR_IntervalNow() - start);
  	   	if (elapsed > timeout) {
  		   	n = 0;  /* timed out */
     	   } else {
        		remaining = timeout - elapsed;
     	   	tv.tv_sec = PR_IntervalToSeconds(remaining);
     		   tv.tv_usec = PR_IntervalToMicroseconds(
        			remaining - PR_SecondsToInterval(tv.tv_sec));
        		goto retry;
         }
  	   }
   }

   if (n > 0) {
   	n = 0;
   	for (pd = pds, epd = pd + npds; pd < epd; pd++) {
   		PRInt32 osfd;
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
   		osfd = bottom->secret->md.osfd;

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
   } else if (n < 0) {
   	err = _MD_ERRNO();
   	if (err == EBADF) {
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
                PR_ASSERT(_MD_ERRNO() == ENOTSOCK);
                if (_MD_ERRNO() == ENOTSOCK) {
                    pd->out_flags = PR_POLL_NVAL;
                    n++;
                }
            }
   		}
   		PR_ASSERT(n > 0);
   	} else {
   		PR_ASSERT(err != EINTR);  /* should have been handled above */
   		_PR_MD_MAP_SELECT_ERROR(err);
      }
   }
   return n;
 }

