/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 *
 * This file implements the socket polling functions that
 * allow us to quickly determine if there is anything for
 * netlib to do at any instance in time.  These functions
 * previously resided in the FE's but now have been consolidated
 * in the backend as part of the NSPR20 transition, since we
 * are no longer using winsock async socket notification.
 *
 * Designed and implemented by Lou Montulli '98
 */
#include "mkutils.h"
#include "mkselect.h"
#include "nslocks.h"

typedef enum {
	ConnectSelect,
	ReadSelect
} SelectType;

/* define this to use FE timers for the call_netlib_all_the_time
 * function.  Otherwise we will use NET_PollSockets to perform the
 * same functionality
 */
#ifdef XP_WIN
#undef USE_TIMERS_FOR_CALL_ALL_THE_TIME
#else
#define USE_TIMERS_FOR_CALL_ALL_THE_TIME
#endif

#define MAX_SIMULTANIOUS_SOCKETS 100
/* should never have more than MAX sockets */
PRPollDesc poll_desc_array[MAX_SIMULTANIOUS_SOCKETS];
unsigned int fd_set_size = 0;               
PRIVATE int net_calling_all_the_time_count=0;
PRIVATE XP_Bool net_slow_timer_on=FALSE;

#if defined(NO_NETWORK_POLLING)
#include "private/primpl.h"

#if defined(XP_PC)
#include "md/_win95.h"

extern HWND gDNSWindow;
extern UINT gMSGAsyncSelect;

/*
 * Helper routine which extracts the OS specific socket from the PRFileDesc
 * structure...
 */
PRInt32
net_GetOSFDFromFileDesc(PRFileDesc* fd)
{
  PRInt32 osfd = 0;
  PRFileDesc *bottom;
  bottom = PR_GetIdentitiesLayer(fd, PR_NSPR_IO_LAYER);

  PR_ASSERT(NULL != bottom);  /* what to do about that? */
  if ((NULL != bottom) && 
      (_PR_FILEDESC_OPEN == bottom->secret->state)) {
      osfd = bottom->secret->md.osfd;
  }
  return osfd;
}

#else /* ! XP_PC */

PRInt32
net_GetOSFDFromFileDesc(PRFileDesc* fd)
{
  PR_ASSERT(0); /* write me */
  return 0;
}
  
#endif /* ! XP_PC */

/*
 * Helper routine which maps the OS specific socket back to its associated
 * PRFileDesc structure...
 */
PRFileDesc* 
net_GetFileDescFromOSFD(PRInt32 osfd)
{
  int i;
  PRFileDesc* fd = NULL;

  /* 
   * Make sure this function is called inside of the LIBNET lock since
   * it modifies global data structures...
   */
  PR_ASSERT(LIBNET_IS_LOCKED());

  for (i=0; i<fd_set_size; i++) {
    fd = poll_desc_array[i].fd;
    if (net_GetOSFDFromFileDesc(fd) == osfd) {
      break;
    }
  }
  return fd;
}

/*
 * Request an asynchronous notification when data is available on the
 * given PRFileDesc...
 */
void
net_AsyncSelect(PRFileDesc* fd, long netMask)
{
  SOCKET osfd;

  osfd = (SOCKET)net_GetOSFDFromFileDesc(fd);

  if (0 != osfd) {
#if defined(XP_PC)
    WSAAsyncSelect(osfd, gDNSWindow, gMSGAsyncSelect, netMask);
#else   /* ! XP_PC */
    PR_ASSERT(0); /* write me */
#endif  /* ! XP_PC */
  } 
}


/*
 * Force the Netlib thread to drop out of its wait state...
 */
void
net_ForceSelect(void)
{
#if defined(XP_PC)
  PostMessage(gDNSWindow, gMSGAsyncSelect, 0, 0L);
#else   /* ! XP_PC */
    PR_ASSERT(0); /* write me */
#endif  /* ! XP_PC */
}

#endif /* NO_NETWORK_POLLING */


/*  Add a select entry, no duplicates. */
PRIVATE void 
net_add_select(SelectType stType, PRFileDesc *prFD)
{
	unsigned int index = fd_set_size;
	unsigned int count;

#define CONNECT_FLAGS (PR_POLL_READ | PR_POLL_EXCEPT | PR_POLL_WRITE)
#define READ_FLAGS    (PR_POLL_READ | PR_POLL_EXCEPT)

  /* 
   * Make sure this function is called inside of the LIBNET lock since
   * it modifies global data structures...
   */
  PR_ASSERT(LIBNET_IS_LOCKED());

    /*  Go through the list, make sure that there is not already an entry for fd. */
	for(count=0; count < fd_set_size; count++)
	{
		if(poll_desc_array[count].fd == prFD)
		{
			/* found it */
			/* verify that it has the same flags that we wan't,  
			 * otherwise add it again with different flags
			 */
			if((stType == ConnectSelect && poll_desc_array[count].in_flags == CONNECT_FLAGS)
			   || (stType == ReadSelect && poll_desc_array[count].in_flags == READ_FLAGS))
			{
				index = count;
				break;
			}
		}
	}

	/* need to add a new one if we didnt' find the index */
	if(index == fd_set_size)
	{
		poll_desc_array[fd_set_size].fd = prFD;
		fd_set_size++;
	}
	
	if(stType == ConnectSelect)
		poll_desc_array[index].in_flags = CONNECT_FLAGS;
	else if(stType == ReadSelect)
		poll_desc_array[index].in_flags = READ_FLAGS;
	else
		PR_ASSERT(0);

#if defined(NO_NETWORK_POLLING)
  /* Request a notification when data is available on the socket... */
  net_AsyncSelect(prFD, 
                  FD_READ | FD_WRITE | FD_ACCEPT | FD_CONNECT | FD_CLOSE | FD_OOB);
#endif /* NO_NETWORK_POLLING */
}

/*  Remove a select if it exists. */
PRIVATE void 
net_remove_select(SelectType stType, PRFileDesc *prFD)
{
	unsigned int count;

  /* 
   * Make sure this function is called inside of the LIBNET lock since
   * it modifies global data structures...
   */
  PR_ASSERT(LIBNET_IS_LOCKED());

    /*  Go through the list */
	for(count=0; count < fd_set_size; count++)
	{
		if(poll_desc_array[count].fd == prFD)
		{
			
			if((stType == ConnectSelect && poll_desc_array[count].in_flags == CONNECT_FLAGS)
			   || (stType == ReadSelect && poll_desc_array[count].in_flags == READ_FLAGS))
			{
				/* found it collapse the list */
				fd_set_size--;
        if(count < fd_set_size) {
					memmove(&poll_desc_array[count], &poll_desc_array[count+1], (fd_set_size - count) * sizeof(PRPollDesc));
        }

#if defined(NO_NETWORK_POLLING)
        /* Cancel any outstanding notification requests for the socket... */
        net_AsyncSelect(prFD, 0);
#endif /* NO_NETWORK_POLLING */
				return;
			}
		}
	}

	/* didn't find it.  opps */
}

MODULE_PRIVATE void 
NET_SetReadPoll(PRFileDesc *fd) 
{
    net_add_select(ReadSelect, fd);
}

MODULE_PRIVATE void 
NET_ClearReadPoll(PRFileDesc *fd) 
{
    net_remove_select(ReadSelect, fd);
}

MODULE_PRIVATE void 
NET_SetConnectPoll(PRFileDesc *fd) 
{
    net_add_select(ConnectSelect, fd);
}

MODULE_PRIVATE void 
NET_ClearConnectPoll(PRFileDesc *fd) 
{
    net_remove_select(ConnectSelect, fd);
}

/* call PR_Poll and call Netlib if necessary 
 *
 * return FALSE if nothing to do.
 */
PUBLIC XP_Bool 
NET_PollSockets(void)
{
	static PRIntervalTime interval = 0;
	register unsigned int itmp;

#if defined(NO_NETWORK_POLLING)
  /*
   * This routine should only be called if network polling is enabled...
   */
  PR_ASSERT(0);
#endif /* NO_NETWORK_POLLING */

  /*
   * Enter the LIBNET lock to protect the poll_desc_array and other global
   * data structures used by NET_PollSockets(...)
   */
  LIBNET_LOCK();

	if(net_calling_all_the_time_count)
  		NET_ProcessNet(NULL, NET_EVERYTIME_TYPE);
		
	if(!interval)
		interval = PR_MillisecondsToInterval(1); 

  if(1 > fd_set_size) {
    LIBNET_UNLOCK();
		return FALSE;
  }

  itmp = PR_Poll(poll_desc_array, fd_set_size, interval);

  if(itmp < 1) {
    LIBNET_UNLOCK();
		return TRUE; /* potential for doing stuff in the future. */
  }

	/* for now call on all active sockets. */
	/* if this is too much call only one, but reorder the list each time. */
	for(itmp=0; itmp < fd_set_size; itmp++)
	{
		if(poll_desc_array[itmp].out_flags)
			NET_ProcessNet(poll_desc_array[itmp].fd, NET_SOCKET_FD);
	}

  LIBNET_UNLOCK();
	return TRUE;
}

void 
net_process_net_timer_callback(void *closure)
{
    PR_ASSERT(net_calling_all_the_time_count >= 0);
    if (net_calling_all_the_time_count == 0)
        return;

    if (NET_ProcessNet(NULL, NET_EVERYTIME_TYPE) == 0)
        net_calling_all_the_time_count = 0;
    else
        FE_SetTimeout(net_process_net_timer_callback, NULL, 1);
}

MODULE_PRIVATE void
NET_SetCallNetlibAllTheTime(MWContext *context, char *caller)
{
    PR_ASSERT(net_calling_all_the_time_count >= 0);

#ifdef USE_TIMERS_FOR_CALL_ALL_THE_TIME
    if (net_calling_all_the_time_count == 0)
        FE_SetTimeout(net_process_net_timer_callback, NULL, 1);
#endif /* USE_TIMERS_FOR_CALL_ALL_THE_TIME */
	
	net_calling_all_the_time_count++;

#if defined(NO_NETWORK_POLLING)
  /*
   * Force the netlib thread to drop out of its wait state (ie. select).
   * This is necessary, since this function can be called on multiple
   * threads...
   */
  net_ForceSelect();
#endif /* NO_NETWORK_POLLING */
}

#define SLOW_NETLIB_TIMER_INTERVAL_MILLISECONDS 10
void 
net_process_slow_net_timer_callback(void *closure)
{
	if(!NET_ProcessNet(NULL, NET_EVERYTIME_TYPE))
		net_slow_timer_on = FALSE; /* dont reset the timer */
	else if (net_slow_timer_on)
		FE_SetTimeout(net_process_slow_net_timer_callback, 
				NULL, 	
				SLOW_NETLIB_TIMER_INTERVAL_MILLISECONDS);
}

/* this function turns on and off a reasonably slow timer that will
 * push the netlib along even when it doesn't get any onIdle time.
 * this is unfortunately necessary on windows because when a modal
 * dialog is up it won't call the OnIdle loop which is currently the
 * source of our events.
 */
MODULE_PRIVATE void
NET_SetNetlibSlowKickTimer(PRBool set)
{
	if(net_slow_timer_on == set)
		return; /* do nothing */

	net_slow_timer_on = set;

	/* call immediately */
	if(net_slow_timer_on)
		FE_SetTimeout(net_process_slow_net_timer_callback, 
				NULL, 	
				SLOW_NETLIB_TIMER_INTERVAL_MILLISECONDS);
}

MODULE_PRIVATE void
NET_ClearCallNetlibAllTheTime(MWContext *context, char *caller)
{
    PR_ASSERT(net_calling_all_the_time_count > 0);
	net_calling_all_the_time_count--;
}

MODULE_PRIVATE PRBool
NET_IsCallNetlibAllTheTimeSet(MWContext *context, char *caller)
{
	if(caller == NULL)
	{
		if(net_calling_all_the_time_count > 0)
			return PR_TRUE;
	}
	else
	{
		/* not implemented */
		PR_ASSERT(0);
	}

	return PR_FALSE;
}

MODULE_PRIVATE void
NET_ClearDNSSelect(MWContext *context, PRFileDesc *file_desc)
{
#if defined(XP_WIN) || (defined(XP_UNIX) && defined(UNIX_ASYNC_DNS)) || (defined(XP_OS2) && !defined(XP_OS2_DOUGSOCK))
      /* FE_AbortDNSLookup(file_desc);  */
#endif /* XP_WIN || (XP_UNIX && UNIX_ASYNC_DNS) || (defined(XP_OS2) && !defined(XP_OS2_DOUGSOCK) */

}

MODULE_PRIVATE void
NET_SetFileReadSelect(MWContext *context, int file_desc)
{
    /* need to conver this over to NSPR PRFileDesc's before it will work */
/*	FE_SetFileReadSelect(context, file_desc);	 */
}

MODULE_PRIVATE void
NET_ClearFileReadSelect(MWContext *context, int file_desc)
{
    /* need to conver this over to NSPR PRFileDesc's before it will work */
/*	FE_ClearReadSelect(context, file_desc);		 */
}

MODULE_PRIVATE void
NET_SetReadSelect(MWContext *context, PRFileDesc *file_desc)
{
	NET_SetReadPoll(file_desc);
}

MODULE_PRIVATE void
NET_ClearReadSelect(MWContext *context, PRFileDesc *file_desc)
{
	NET_ClearReadPoll(file_desc);
}

MODULE_PRIVATE void
NET_SetConnectSelect(MWContext *context, PRFileDesc *file_desc)
{
	NET_SetConnectPoll(file_desc);
}

MODULE_PRIVATE void
NET_ClearConnectSelect(MWContext *context, PRFileDesc *file_desc)
{
	NET_ClearConnectPoll(file_desc);
}
