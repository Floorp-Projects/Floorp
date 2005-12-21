/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "nsToolkit.h"
#include "prmon.h"
#include "prtime.h"
#include "nsGUIEvent.h"
#include "nsSwitchToUIThread.h"
#include "plevent.h"
#include "prprf.h"

// 
// Static thread local storage index of the Toolkit 
// object associated with a given thread... 
// 
static PRUintn gToolkitTLSIndex = 0; 

//-------------------------------------------------------------------------
//
// nsISupports implementation macro
//
//-------------------------------------------------------------------------
NS_IMPL_THREADSAFE_ISUPPORTS1(nsToolkit, nsIToolkit)

struct ThreadInterfaceData
{
  void	*data;
  thread_id waitingThread;
};

//
// main for the message pump thread
//
PRBool gThreadState = PR_FALSE;

struct ThreadInitInfo {
  PRMonitor *monitor;
  nsToolkit *toolkit;
};

void nsToolkit::RunPump(void* arg)
{
  int32		code;
  char		portname[64];
  ThreadInterfaceData id;

  ThreadInitInfo *info = (ThreadInitInfo*)arg;
  PR_EnterMonitor(info->monitor);

  gThreadState = PR_TRUE;

  PR_Notify(info->monitor);
  PR_ExitMonitor(info->monitor);

  delete info;

  // system wide unique names
  PR_snprintf(portname, sizeof(portname), "event%lx", 
              (long unsigned) PR_GetCurrentThread());

  port_id event = create_port(100, portname);

  while(read_port(event, &code, &id, sizeof(id)) >= 0)
  {
    switch(code)
    {
      case WM_CALLMETHOD :
        {
          MethodInfo *mInfo = (MethodInfo *)id.data;
          mInfo->Invoke();
          if(id.waitingThread != 0)
            resume_thread(id.waitingThread);
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
  PR_SetThreadPrivate(gToolkitTLSIndex, nsnull);
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
	
  PR_EnterMonitor(monitor);
	
  ThreadInitInfo *ti = new ThreadInitInfo();
  if (ti)
  {
    ti->monitor = monitor;
    ti->toolkit = this;
  
    // create a gui thread
    mGuiThread = PR_CreateThread(PR_SYSTEM_THREAD,
                                   RunPump,
                                   (void*)ti,
                                   PR_PRIORITY_HIGH,
                                   PR_LOCAL_THREAD,
                                   PR_UNJOINABLE_THREAD,
                                   0);

    // wait for the gui thread to start
    while(gThreadState == PR_FALSE)
    {
      PR_Wait(monitor, PR_INTERVAL_NO_TIMEOUT);
    }
  }
    
  // at this point the thread is running
  PR_ExitMonitor(monitor);
  PR_DestroyMonitor(monitor);
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
  if (NULL != aThread) 
  {
    mGuiThread = aThread;
    localthread = false;
  } 
  else 
  {
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
    char portname[64];

    PR_snprintf(portname, sizeof(portname), "event%lx", 
                (long unsigned) mGuiThread);

    eventport = find_port(portname);

    cached = true;
  }
}

void nsToolkit::CallMethod(MethodInfo *info)
{
  ThreadInterfaceData id;

  GetInterface();

  id.data = info;
  id.waitingThread = find_thread(NULL);
  if(write_port(eventport, WM_CALLMETHOD, &id, sizeof(id)) == B_OK)
  {
    // semantics for CallMethod are that it should be synchronous
    suspend_thread(id.waitingThread);
  }
}

// to be used only from a BView or BWindow
void nsToolkit::CallMethodAsync(MethodInfo *info)
{
  ThreadInterfaceData id;

  GetInterface();

  id.data = info;
  id.waitingThread = 0;
	
  // Check message count to not exceed the port's capacity.
  // There seems to be a BeOS bug that allows more 
  // messages on a port than its capacity.
  port_info portinfo;
  if (get_port_info(eventport, &portinfo) != B_OK)
  {
    return;
  }
  
  if (port_count(eventport) < portinfo.capacity - 20) 
  {
    write_port_etc(eventport, WM_CALLMETHOD, &id, sizeof(id), B_TIMEOUT, 0);
  }
}

//------------------------------------------------------------------------- 
// 
// Return the nsIToolkit for the current thread.  If a toolkit does not 
// yet exist, then one will be created... 
// 
//------------------------------------------------------------------------- 
NS_METHOD NS_GetCurrentToolkit(nsIToolkit* *aResult) 
{ 
  nsIToolkit* toolkit = nsnull; 
  nsresult rv = NS_OK; 
  PRStatus status; 

  // Create the TLS index the first time through... 
  if (0 == gToolkitTLSIndex)  
  { 
    status = PR_NewThreadPrivateIndex(&gToolkitTLSIndex, NULL); 
    if (PR_FAILURE == status) 
    { 
      rv = NS_ERROR_FAILURE; 
    } 
  } 

  if (NS_SUCCEEDED(rv)) 
  { 
    toolkit = (nsIToolkit*)PR_GetThreadPrivate(gToolkitTLSIndex); 

    // 
    // Create a new toolkit for this thread... 
    // 
    if (!toolkit) 
    { 
      toolkit = new nsToolkit(); 

      if (!toolkit) 
      { 
        rv = NS_ERROR_OUT_OF_MEMORY; 
      } 
      else 
      { 
        NS_ADDREF(toolkit); 
        toolkit->Init(PR_GetCurrentThread()); 
        // 
        // The reference stored in the TLS is weak.  It is removed in the 
        // nsToolkit destructor... 
        // 
        PR_SetThreadPrivate(gToolkitTLSIndex, (void*)toolkit); 
      } 
    } 
    else 
    { 
      NS_ADDREF(toolkit); 
    } 
    *aResult = toolkit; 
  } 

  return rv; 
} 
