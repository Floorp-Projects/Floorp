/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsToolkit.h"
#include "nsWindow.h"
#include "prmon.h"
#include "prtime.h"
#include "nsITimer.h"
#include "nsGUIEvent.h"
#include "nsSwitchToUIThread.h"
#include "plevent.h"

//NS_IMPL_ISUPPORTS(nsToolkit, NS_ITOOLKIT_IID)
//-------------------------------------------------------------------------
//
// nsISupports implementation macro
//
//-------------------------------------------------------------------------
NS_DEFINE_IID(kIToolkitIID, NS_ITOOLKIT_IID);
NS_IMPL_ISUPPORTS(nsToolkit,kIToolkitIID);

struct ThreadInterfaceData
{
	void	*data;
	int32	sync;
};

//
// main for the message pump thread
//
PRBool gThreadState = PR_FALSE;

static sem_id my_find_sem(const char *name)
{
	sem_id	ret = B_ERROR;

	/* Get the sem_info for every sempahore in this team. */
	sem_info info;
	int32 cookie = 0;

	while(get_next_sem_info(0, &cookie, &info) == B_OK)
		if(strcmp(name, info.name) == 0)
		{
			ret = info.sem;
			break;
		}

	return ret;
}

struct ThreadInitInfo {
    PRMonitor *monitor;
    nsToolkit *toolkit;
};

void nsToolkit::RunPump(void* arg)
{
	int32		code;
	char		portname[64];
	char		semname[64];
	ThreadInterfaceData id;

	ThreadInitInfo *info = (ThreadInitInfo*)arg;
	::PR_EnterMonitor(info->monitor);

	gThreadState = PR_TRUE;

	::PR_Notify(info->monitor);
	::PR_ExitMonitor(info->monitor);

	delete info;

	// system wide unique names
	sprintf(portname, "event%lx", PR_GetCurrentThread());
	sprintf(semname, "sync%lx", PR_GetCurrentThread());

	port_id	event = create_port(100, portname);
	sem_id	sync = create_sem(0, semname);

	while(read_port(event, &code, &id, sizeof(id)) >= 0)
	{
		switch(code)
		{
			case 'WMti' :
				extern void nsTimerExpired(void *);	// hack: this is in gfx
				nsTimerExpired(id.data);
				break;

			case WM_CALLMETHOD :
				{
				MethodInfo *mInfo = (MethodInfo *)id.data;
				mInfo->Invoke();
				if(! id.sync)
					delete mInfo;
				}
				break;

			case 'natv' :	// native queue PLEvent
				{
				PREventQueue *queue = (PREventQueue *)id.data;
				PR_ProcessPendingEvents(queue);
				}
				break;

			default :
				printf("nsToolkit::RunPump - UNKNOWN EVENT\n");
				break;
		}

		if(id.sync)
			release_sem(sync);
	}
}

//-------------------------------------------------------------------------
//
// constructor
//
//-------------------------------------------------------------------------
nsToolkit::nsToolkit()  
{
	localthread = false;
    NS_INIT_REFCNT();
    mGuiThread  = NULL;
}


//-------------------------------------------------------------------------
//
// destructor
//
//-------------------------------------------------------------------------
nsToolkit::~nsToolkit()
{
	Kill();
}

void nsToolkit::Kill()
{
	if(localthread)
	{
		GetInterface();

		// interrupt message flow
		close_port(eventport);
	}
}

//-------------------------------------------------------------------------
//
// Create a new thread and run the message pump in there
//
//-------------------------------------------------------------------------
void nsToolkit::CreateUIThread()
{
	PRMonitor *monitor = ::PR_NewMonitor();
	
	::PR_EnterMonitor(monitor);
	
	ThreadInitInfo *ti = new ThreadInitInfo();
	ti->monitor = monitor;
	ti->toolkit = this;

	// create a gui thread
	mGuiThread = ::PR_CreateThread(PR_SYSTEM_THREAD,
					RunPump,
					(void*)ti,
					PR_PRIORITY_NORMAL,
					PR_LOCAL_THREAD,
					PR_UNJOINABLE_THREAD,
					0);

	// wait for the gui thread to start
	while(gThreadState == PR_FALSE)
		::PR_Wait(monitor, PR_INTERVAL_NO_TIMEOUT);

	// at this point the thread is running
	::PR_ExitMonitor(monitor);
	::PR_DestroyMonitor(monitor);
}


//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD nsToolkit::Init(PRThread *aThread)
{
	Kill();

	// Store the thread ID of the thread containing the message pump.  
	// If no thread is provided create one
	if (NULL != aThread) {
		mGuiThread = aThread;
		localthread = false;
	} else {
		localthread = true;

		// create a thread where the message pump will run
		CreateUIThread();
	}

	cached = false;

	return NS_OK;
}

void nsToolkit::GetInterface()
{
	if(! cached)
	{
		char		portname[64];
		char		semname[64];

		sprintf(portname, "event%lx", mGuiThread);
		sprintf(semname, "sync%lx", mGuiThread);

		eventport = find_port(portname);
		syncsem = my_find_sem(semname);

		cached = true;
	}
}

void nsToolkit::CallMethod(MethodInfo *info)
{
	ThreadInterfaceData	 id;

	GetInterface();

	id.data = info;
	id.sync = true;
	if(write_port(eventport, WM_CALLMETHOD, &id, sizeof(id)) == B_OK)
	{
		// semantics for CallMethod are that it should be synchronous
		while(acquire_sem(syncsem) == B_INTERRUPTED)
			;
	}
}

// to be used only from a BView or BWindow
void nsToolkit::CallMethodAsync(MethodInfo *info)
{
	ThreadInterfaceData	 id;

	GetInterface();

	id.data = info;
	id.sync = false;
	write_port_etc(eventport, WM_CALLMETHOD, &id, sizeof(id), B_TIMEOUT, 0);
}
