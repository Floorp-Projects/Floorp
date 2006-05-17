/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Duncan Wilcox <duncan@be.com>
 *   Yannick Koehler <ykoehler@mythrium.com>
 *   Makoto Hamanaka <VYA04230@nifty.com>
 *   Fredrik Holmqvist <thesuckiestemail@yahoo.se>
 *   Sergei Dolgov <sergei_d@fi.tartu.ee>
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

#include "nsAppShell.h"
#include "nsSwitchToUIThread.h"

#include "prprf.h"

#include <Application.h>
#include <stdlib.h>


static sem_id my_find_sem(const char *name)
{
	sem_id	ret = B_ERROR;
	/* Get the sem_info for every sempahore in this team. */
	sem_info info;
	int32 cookie = 0;
	while(get_next_sem_info(0, &cookie, &info) == B_OK)
	{
		if(strcmp(name, info.name) == 0)
		{
			ret = info.sem;
			break;
		}
	}
	return ret;
}


//-------------------------------------------------------------------------
//
// nsAppShell constructor
//
//-------------------------------------------------------------------------
nsAppShell::nsAppShell()  
	: is_port_error(false)
{ 
	eventport = nsnull;
}


//-------------------------------------------------------------------------
//
// Create the application shell
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsAppShell::Init()
{
	// system wide unique names
	// NOTE: this needs to be run from within the main application thread
	
	char		portname[64];
	char		semname[64];
	PR_snprintf(portname, sizeof(portname), "event%lx", (long unsigned) PR_GetCurrentThread());
	PR_snprintf(semname, sizeof(semname), "sync%lx", (long unsigned) PR_GetCurrentThread());
              
#ifdef DEBUG              
	printf("nsAppShell::Create portname: %s, semname: %s\n", portname, semname);
#endif
	/* 
	* Set up the port for communicating. As restarts thru execv may occur
	* and ports survive those (with faulty events as result). We need to take extra
	* care that the port is created for this launch, otherwise we need to reopen it
	* so that faulty messages gets lost.
	*
	* We do this by checking if the sem has been created. If it is we can reuse the port (if it exists).
	* Otherwise we need to create the sem and the port, deleting any open ports before.
	* TODO: The semaphore is no longer needed for syncing, so it's only use is for detecting if the
	* port needs to be reopened. This should be replaced, but I'm not sure how -tqh
	*/
	syncsem = my_find_sem(semname);
	eventport = find_port(portname);
	if (B_ERROR != syncsem) 
	{
		if (eventport < 0)
		{
			eventport = create_port(200, portname);
		}
		return nsBaseAppShell::Init();
	} 
	
	if (eventport >= 0)
	{
		delete_port(eventport);
	}
	eventport = create_port(200, portname);
	syncsem = create_sem(0, semname);
	return nsBaseAppShell::Init();
}



//-------------------------------------------------------------------------
//
// nsAppShell destructor
//
//-------------------------------------------------------------------------
nsAppShell::~nsAppShell()
{
	close_port(eventport);
	delete_port(eventport);
	delete_sem(syncsem);
	
	if (be_app->Lock())
	{
		be_app->Quit();
	}
}

// This version ignores mayWait flag totally
PRBool nsAppShell::ProcessNextNativeEvent(PRBool mayWait)
{
	bool gotMessage = false;

	// should we check for eventport initialization ?
	if (eventport == 0)
	{
		char  portname[64];	
		PR_snprintf(portname, sizeof(portname), "event%lx", (long unsigned) PR_GetCurrentThread());
		// XXX - Do we really need to search eventport every time? There is one istance of AppShell per app
		// and finding it once in Init() might be sufficient.
		// At least version this version with if(!eventport) worked for me well in tests.
		// It may add some performance, sprintf with format is bit expensive for huge message flood.
		if((eventport = find_port(portname)) < 0)
		{
	// not initialized
#ifdef DEBUG
			printf("nsAppShell::DispatchNativeEvent() was called before init\n");
#endif
			return gotMessage;
		}
	}

	// Previously we collected events in RetrieveAllEvents via simple read_port,
	// and unblocking was performed by sending fake 'natv' message from _pl_NativeNotify()
	
	// Currently we ignoring event type, previously we had 5 priority levels, different for mouse, kbd etc.
	// MS Windows code sets some priority for kbd and IME events by ignoring mayWait
	// There is huge need now to rewrite MouseMove handling in nsWindow.cpp.

	
	// mayWait here blocked app totally until I replaced read_port with read_port_etc.
	// I suspect it may be related to non-implemented ScheduleNativeEventCallback()
	if (port_count(eventport) || mayWait) 
	{
#ifdef DEBUG
		printf("PNNE\n");
#endif
		EventItem *newitem = new EventItem;
		if (!newitem)
			return gotMessage;
		newitem->code = 0;
		newitem->ifdata.data = nsnull;
		newitem->ifdata.waitingThread = 0;
		// Here is interesting thing, according to logic described in
		// https://bugzilla.mozilla.org/show_bug.cgi?id=337550#c17
		// read_port() is what should be here, if we take mayWait in account, 
		// but application even don't start with that. Or, if it does, blocks in first window created.
		// Using read_port_etc with real timeout allows it work as expected.
		// Second interesting fact, if we don't use || mayWait condition, CPU load depends on timeout value -
		// smaller value, bigger CPU load.
		if (read_port_etc(eventport, &newitem->code, &newitem->ifdata, sizeof(newitem->ifdata), B_TIMEOUT, 100000) < 0)
		{
			delete newitem;
			newitem = nsnull;
			is_port_error = true;
			return gotMessage;
		}
		gotMessage = true;
		InvokeBeOSMessage(newitem);
		delete newitem;
		newitem = nsnull;
#ifdef DEBUG
		printf("Retrived ni = %p\n", newitem);
#endif

	}
	return gotMessage;
}
//-------------------------------------------------------------------------

void nsAppShell::ScheduleNativeEventCallback()
{
#ifdef DEBUG
	printf("SNEC\n");
#endif
// ??? what to do here? Invoke fake message like that 'natv' from gone plevent ?
}

void nsAppShell::InvokeBeOSMessage(EventItem *item)
{
		int32 code;
		ThreadInterfaceData id;
		id.data = 0;
		id.waitingThread = 0;
		code = item->code;
		id = item->ifdata;
		MethodInfo *mInfo = (MethodInfo *)id.data;
		mInfo->Invoke();
		if (id.waitingThread != 0)
		{
			resume_thread(id.waitingThread);
		}
		delete mInfo;
}