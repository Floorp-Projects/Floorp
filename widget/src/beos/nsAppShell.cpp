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

//-------------------------------------------------------------------------
//
// nsAppShell constructor
//
//-------------------------------------------------------------------------
nsAppShell::nsAppShell()  
	: is_port_error(false), scheduled (false)
{ 
	eventport = -1; // 0 is legal value for port_id!
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
	
	char portname[B_OS_NAME_LENGTH];
	PR_snprintf(portname, sizeof(portname), "event%lx", (long unsigned) PR_GetCurrentThread());
             
#ifdef DEBUG              
	printf("nsAppShell::Create portname: %s\n", portname);
#endif

	// Clean up things. Restart process in toolkit may leave old port alive.
	if ((eventport = find_port(portname)) >= 0)
	{
		close_port(eventport);
		delete_port(eventport);
	}
		
	eventport = create_port(200, portname);
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

	if (be_app->Lock())
	{
		be_app->Quit();
	}
}

PRBool nsAppShell::ProcessNextNativeEvent(PRBool mayWait)
{
	bool gotMessage = false;

	// should we check for eventport initialization ?
	if (eventport < 0)
	{
		char  portname[B_OS_NAME_LENGTH];	
		PR_snprintf(portname, sizeof(portname), "event%lx", (long unsigned) PR_GetCurrentThread());
		// XXX - Do we really need to search eventport every time? There is one istance of AppShell per app
		// and finding it once in Init() might be sufficient.
		if((eventport = find_port(portname)) < 0)
		{
			// not initialized
#ifdef DEBUG
			printf("nsAppShell::DispatchNativeEvent() was called before init\n");
#endif
			return gotMessage;
		}
	}
	
	// Currently we ignoring event type, previously we had 5 priority levels, different for mouse, kbd etc.
	// MS Windows code sets some priority for kbd and IME events by ignoring mayWait

	if (port_count(eventport))
		gotMessage = InvokeBeOSMessage(0);

	// There is next message in port queue
	if (port_count(eventport) && !mayWait)
	{
		if (!scheduled)
		{
			// There is new (not scheduled) event and we cannot wait.
			// So inform AppShell about existence of new event.
			// Actually it should be called from something like BLooper::MessageReceived(),
			// or, in our case, in nsToolkit CallMethod*() 
			NativeEventCallback();
		}
		else
		{
			scheduled = false;
			gotMessage = InvokeBeOSMessage(0);
		}
	}
	// Hack. Emulating logic for mayWait.
	// Allow next event to pass through (if it appears inside 100000 uS)
	// Actually it should block and then be unblocked from independent thread
	// In that case read_port() should be used.
	if (mayWait)
		gotMessage = InvokeBeOSMessage(100000);
	return gotMessage;
}
//-------------------------------------------------------------------------

void nsAppShell::ScheduleNativeEventCallback()
{
	if (eventport < 0)
		return;
	port_info portinfo;
	if (get_port_info(eventport, &portinfo) != B_OK)
		return;
	if (port_count(eventport) < portinfo.capacity - 20)
	{
		// This should be done from different thread in reality in order to unblock
		ThreadInterfaceData id;
		id.data = 0;
		id.waitingThread = find_thread(NULL);
		write_port_etc(eventport, 'natv', &id, sizeof(id), B_TIMEOUT, 1000000);
		scheduled = true;
	}
}

bool nsAppShell::InvokeBeOSMessage(bigtime_t timeout)
{
	int32 code;
	ThreadInterfaceData id;
	if (read_port_etc(eventport, &code, &id, sizeof(id), B_TIMEOUT, timeout) < 0)
	{
		is_port_error = true;
		return false;
	}

	id.waitingThread = 0; 
	MethodInfo *mInfo = (MethodInfo *)id.data;
	if (code != 'natv')
		mInfo->Invoke();

	if (id.waitingThread != 0)
		resume_thread(id.waitingThread);
	delete mInfo;
	return true;
}