/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsAppShell.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsIWidget.h"
#include "nsIAppShell.h"
#include "nsWindow.h"
#include "nsSwitchToUIThread.h"
#include "nsTimerBeOS.h"
#include "plevent.h"
#include "prprf.h"

#include <stdlib.h>
#include <AppKit.h>
#include <AppFileInfo.h>

static int gBAppCount = 0;

struct ThreadInterfaceData
{
  void  *data;
  int32  sync;
};

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
// nsISupports implementation macro
//
//-------------------------------------------------------------------------
NS_DEFINE_IID(kIAppShellIID, NS_IAPPSHELL_IID);
NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
NS_IMPL_ISUPPORTS(nsAppShell,kIAppShellIID);

static bool GetAppSig(char *sig)
{
  app_info 		appInfo;
  BFile 			file;
  BAppFileInfo	appFileInfo;
  image_info		info;
  int32			cookie = 0;

  *sig = 0;
  return get_next_image_info(0, &cookie, &info) == B_OK &&
         file.SetTo(info.name, B_READ_ONLY) == B_OK &&
         appFileInfo.SetTo(&file) == B_OK &&
         appFileInfo.GetSignature(sig) == B_OK;
}

class nsBeOSApp : public BApplication
{
  public:
    sem_id init;

    nsBeOSApp(const char *signature, sem_id initsem);
    virtual void ReadyToRun(void);
};

nsBeOSApp::nsBeOSApp(const char *signature, sem_id initsem)
 : BApplication(signature), init(initsem)
{
}

void nsBeOSApp::ReadyToRun(void)
{
  release_sem(init);
}

// Return B_OK for success, anything else error!
int32 bapp_thread(void *arg)
{
  // create and start BApplication
  char sig[B_MIME_TYPE_LENGTH + 1];
  int32 error = B_OK;
  
  GetAppSig(sig);
  nsBeOSApp	*app = new nsBeOSApp(sig, (sem_id)arg);
  if(app)
  {
    app->Run();
  }
  else
  {
    error = B_NO_MEMORY;
  }
  return error;
}

//-------------------------------------------------------------------------
//
// nsAppShell constructor
//
//-------------------------------------------------------------------------
nsAppShell::nsAppShell()  
{ 
  NS_INIT_REFCNT();
  mDispatchListener = 0;

  if(gBAppCount++ == 0)
  {
    sem_id initsem = create_sem(0, "bapp init");
    resume_thread(spawn_thread(bapp_thread, "BApplication", B_NORMAL_PRIORITY, (void *)initsem));
    acquire_sem(initsem);
    delete_sem(initsem);
  }
}


//-------------------------------------------------------------------------
//
// Create the application shell
//
//-------------------------------------------------------------------------

NS_METHOD nsAppShell::Create(int* argc, char ** argv)
{
  // system wide unique names
  // NOTE: this needs to be run from within the main application thread
  char		portname[64];
  char		semname[64];
  PR_snprintf(portname, sizeof(portname), "event%lx", 
              (long unsigned) PR_GetCurrentThread());
  PR_snprintf(semname, sizeof(semname), "sync%lx", 
              (long unsigned) PR_GetCurrentThread());

  if((eventport = find_port(portname)) < 0)
  {
    // we're here first
    eventport = create_port(100, portname);
    syncsem = create_sem(0, semname);
  }
  else
  {
    // the PLEventQueue stuff (in plevent.c) created the queue before we started
    syncsem = my_find_sem(semname);
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsAppShell::SetDispatchListener(nsDispatchListener* aDispatchListener) 
{
  mDispatchListener = aDispatchListener;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Enter a message handler loop
//
//-------------------------------------------------------------------------

nsresult nsAppShell::Run()
{
  int32               code;
  ThreadInterfaceData id;

  NS_ADDREF_THIS();

  while(read_port(eventport, &code, &id, sizeof(id)) >= 0)
  {
    switch(code)
    {
      case 'WMti' :
        {
          // Hack
          nsCOMPtr<nsTimerBeOS> timer = (nsTimerBeOS *)id.data;
          timer->FireTimeout();
        }
        break;

      case WM_CALLMETHOD :
        {
          MethodInfo *mInfo = (MethodInfo *)id.data;
          mInfo->Invoke();
          if(! id.sync)
          {
            delete mInfo;
          }
        }
        break;

      case 'natv' :	// native queue PLEvent
        {
          PREventQueue *queue = (PREventQueue *)id.data;
          PR_ProcessPendingEvents(queue);
        }
        break;

      default :
#ifdef DEBUG
        printf("nsAppShell::Run - UNKNOWN EVENT\n");
#endif
        break;
    }

    if(mDispatchListener)
    {
      mDispatchListener->AfterDispatch();
    }
    
    if(id.sync)
    {
      release_sem(syncsem);
    }
  }

  Release();

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Exit a message handler loop
//
//-------------------------------------------------------------------------

NS_METHOD nsAppShell::Exit()
{
  // interrupt message flow
  close_port(eventport);

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// nsAppShell destructor
//
//-------------------------------------------------------------------------
nsAppShell::~nsAppShell()
{
  if(--gBAppCount == 0)
  {
    if(be_app->Lock())
    {
      be_app->Quit();
    }
  }
}

//-------------------------------------------------------------------------
//
// GetNativeData
//
//-------------------------------------------------------------------------
void* nsAppShell::GetNativeData(PRUint32 aDataType)
{
  // To be implemented.
  return nsnull;
}

//-------------------------------------------------------------------------
//
// Spinup - do any preparation necessary for running a message loop
//
//-------------------------------------------------------------------------
NS_METHOD nsAppShell::Spinup()
{
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Spindown - do any cleanup necessary for finishing a message loop
//
//-------------------------------------------------------------------------
NS_METHOD nsAppShell::Spindown()
{
  return NS_OK;
}

NS_METHOD nsAppShell::GetNativeEvent(PRBool &aRealEvent, void *&aEvent)
{
  aRealEvent = PR_FALSE;
  aEvent = 0;

  return NS_OK;
}

NS_METHOD nsAppShell::DispatchNativeEvent(PRBool aRealEvent, void *aEvent)
{
  // should we check for eventport initialization ?
  char  portname[64];
  PR_snprintf(portname, sizeof(portname), "event%lx", 
              (long unsigned) PR_GetCurrentThread());

  if((eventport = find_port(portname)) < 0) 
  {
    // not initialized
#ifdef DEBUG
    printf("nsAppShell::DispatchNativeEvent() was called before init\n");
#endif
    fflush(stdout);
    return NS_ERROR_FAILURE;
  }

  int32 code;
  ThreadInterfaceData id;
  id.data = 0;
  id.sync = 0;
  bool gotMessage = false;

  do 
  {
    if (read_port(eventport, &code, &id, sizeof(id)) >= 0) 
    {
      switch(code) 
      {
        case 'WMti' :
          {
            // Hack
            nsCOMPtr<nsTimerBeOS> timer = (nsTimerBeOS *)id.data;
            timer->FireTimeout();
          }
          break;
        
        case WM_CALLMETHOD :
          {
            MethodInfo *mInfo = (MethodInfo *)id.data;
            mInfo->Invoke();
            if(! id.sync)
            {
              delete mInfo;
            }
            gotMessage = PR_TRUE;
          }
          break;
        
        case 'natv' :	// native queue PLEvent
          {
            PREventQueue *queue = (PREventQueue *)id.data;
            PR_ProcessPendingEvents(queue);
            gotMessage = PR_TRUE;
          }
          break;
          
        default :
#ifdef DEBUG
          printf("nsAppShell::Run - UNKNOWN EVENT\n");
#endif
          break;
      }

      if(id.sync)
      {
        release_sem(syncsem);
      }
    }
    else
    {
      // read_port failure
#ifdef DEBUG
      printf("nsAppShell::GetNativeEvent() read_port failed.\n");
#endif
      return NS_ERROR_FAILURE;
    }
  } while (!gotMessage);

  // no need to do this?
  //if(mDispatchListener)
  //{
  //	mDispatchListener->AfterDispatch();
  //}
  
  return NS_OK;
}

