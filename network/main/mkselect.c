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

/*  Add a select entry, no duplicates. */
PRIVATE void 
net_add_select(SelectType stType, PRFileDesc *prFD)
{
	unsigned int index = fd_set_size;
	unsigned int count;

#define CONNECT_FLAGS (PR_POLL_READ | PR_POLL_EXCEPT | PR_POLL_WRITE)
#define READ_FLAGS    (PR_POLL_READ | PR_POLL_EXCEPT)

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
		XP_ASSERT(0);
}

/*  Remove a select if it exists. */
PRIVATE void 
net_remove_select(SelectType stType, PRFileDesc *prFD)
{
	unsigned int count;

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
				if(count < fd_set_size)
					XP_MEMCPY(&poll_desc_array[count], &poll_desc_array[count+1], (fd_set_size - count) * sizeof(PRPollDesc));

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

	if(net_calling_all_the_time_count)
  		NET_ProcessNet(NULL, NET_EVERYTIME_TYPE);
		
	if(!interval)
		interval = PR_MillisecondsToInterval(1); 

	if(1 > fd_set_size)
		return FALSE;

	itmp = PR_Poll(poll_desc_array, fd_set_size, interval);

	if(itmp < 1)
		return TRUE; /* potential for doing stuff in the future. */

	/* for now call on all active sockets. */
	/* if this is too much call only one, but reorder the list each time. */
	for(itmp=0; itmp < fd_set_size; itmp++)
	{
		if(poll_desc_array[itmp].out_flags)
			NET_ProcessNet(poll_desc_array[itmp].fd, NET_SOCKET_FD);
	}

	return TRUE;
}

void 
net_process_net_timer_callback(void *closure)
{
	if(!NET_ProcessNet(NULL, NET_EVERYTIME_TYPE))
		return;  /* dont reset the timer */

	if(net_calling_all_the_time_count)
		FE_SetTimeout(net_process_net_timer_callback, NULL, 1);
}

MODULE_PRIVATE void
NET_SetCallNetlibAllTheTime(MWContext *context, char *caller)
{
	if(net_calling_all_the_time_count < 0)
	{
		XP_ASSERT(0);
		net_calling_all_the_time_count = 0;
	}

#ifdef USE_TIMERS_FOR_CALL_ALL_THE_TIME
	FE_SetTimeout(net_process_net_timer_callback, NULL, 1);
#endif /* USE_TIMERS_FOR_CALL_ALL_THE_TIME */
	
	net_calling_all_the_time_count++;
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
NET_SetNetlibSlowKickTimer(XP_Bool set)
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
	if(net_calling_all_the_time_count < 1)
	{
		XP_ASSERT(0);
		net_calling_all_the_time_count = 1;
	}

	net_calling_all_the_time_count--;

}

MODULE_PRIVATE XP_Bool
NET_IsCallNetlibAllTheTimeSet(MWContext *context, char *caller)
{
	if(caller == NULL)
	{
		if(net_calling_all_the_time_count > 0)
			return TRUE;
	}
	else
	{
		/* not implemented */
		XP_ASSERT(0);
	}

	return FALSE;
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
