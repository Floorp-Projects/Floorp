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
#include "nsITimer.h"
#include "nsUnixTimerCIID.h"
#include "nsIComponentManager.h"
#include "nsIUnixToolkitService.h"

#include "nsCRT.h"
#include "prenv.h"

#include "prinrval.h"

static NS_DEFINE_IID(kITimerIID, NS_ITIMER_IID);
static NS_DEFINE_CID(kCTimerGtk, NS_TIMER_GTK_CID);
static NS_DEFINE_CID(kCTimerMotif, NS_TIMER_MOTIF_CID);
static NS_DEFINE_CID(kCTimerXlib, NS_TIMER_XLIB_CID);

// Yes, this debug code is evil cause it uses a static string.  
#ifdef DEBUG_ramiro
static nsString sToolkitName = "WTF";
#endif

static PRTime sCIStartTime = 0;
static PRTime sCIEndTime = 0;

static nsresult NewTimer(const nsCID & aClass,nsITimer ** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  
  if (nsnull == aInstancePtrResult) 
    return NS_ERROR_NULL_POINTER;
  
  nsresult   rv;
  nsITimer * timer = nsnull;


  sCIStartTime = PR_Now();

  rv = nsComponentManager::CreateInstance(aClass,
                                          nsnull,
                                          kITimerIID,
                                          (void **)& timer);


  sCIEndTime = PR_Now();

#ifdef NS_DEBUG
  nsString message;
  
  message = "Couldn't create a ";

#ifdef DEBUG_ramiro
  message += sToolkitName;
  message += " timer";
#else
  message += "timer";
#endif

  NS_ASSERTION(NS_SUCCEEDED(rv), (const char *) nsAutoCString(message));
#endif


  if (nsnull == timer) 
    return NS_ERROR_OUT_OF_MEMORY;
  
  return timer->QueryInterface(kITimerIID, (void **) aInstancePtrResult);
}
//////////////////////////////////////////////////////////////////////////
static const nsCID *
GetTimerCID()
{
  static const nsCID * sgTimerCID = nsnull;

  static NS_DEFINE_CID(kCUnixToolkitServiceCID, NS_UNIX_TOOLKIT_SERVICE_CID);

  // For obvious efficiency reasons, do this only once
  if (nsnull == sgTimerCID)
  {
    nsIUnixToolkitService * unixToolkitService = nsnull;

    nsresult rv = 
      nsComponentManager::CreateInstance(kCUnixToolkitServiceCID,
                                         nsnull,
                                         nsIUnixToolkitService::GetIID(),
                                         (void **) &unixToolkitService);
    
    NS_ASSERTION(rv == NS_OK,"Cannot obtain unix toolkit service.");
    
    if (NS_OK == rv && nsnull != unixToolkitService)
    {
      nsresult rv2;

      rv2 = unixToolkitService->GetTimerCID((nsCID **) &sgTimerCID);
      
      NS_ASSERTION(rv2 == NS_OK,"Cannot get timer cid.");
      
      NS_RELEASE(unixToolkitService);
    }
  }

  return sgTimerCID;
}
//////////////////////////////////////////////////////////////////////////

static PRTime sStartTime = 0;
static PRTime sEndTime = 0;
static int sTimerCount = 0;
//////////////////////////////////////////////////////////////////////////
nsresult NS_NewTimer(nsITimer ** aInstancePtrResult)
{
  sStartTime = PR_Now();
  const nsCID * cid = GetTimerCID();

  NS_ASSERTION(nsnull != cid,"Dude! Trying to make a timer with a null CID.");

  nsresult rv = NewTimer(*cid,aInstancePtrResult);

  sEndTime = PR_Now();

#if 0
  printf("NS_NewTimer(count=%-4d,time=%lld ms,ci=%lld ms)\n",
         sTimerCount++,
         sEndTime - sStartTime,
         sCIEndTime - sCIStartTime);
#endif

  return rv;
}
