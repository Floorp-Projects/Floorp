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

// Cache the nsIFactory vs. nsComponentManager::CreateInstance()
// The CM is supposed to cache the factory itself, but hashing into
// that doubles the time NS_NewTimer() takes.
//
// With factory caching, the time used by this NS_NewTimer() compared
// to the monolithic ones, is very close.  Theres only a few (2/3) ms
// difference in optimized builds.  This is probably within the
// margin of error of PR_Now().
#define CACHE_FACTORY

#undef CHEAP_PERFORMANCE_MEASURMENT

// Used to measure how long NS_NewTimer() and CreateInstance() takes.
#ifdef CHEAP_PERFORMANCE_MEASURMENT
#include "prinrval.h"
static PRTime sStartTime = 0;
static PRTime sEndTime = 0;
static PRTime sCreateInstanceStartTime = 0;
static PRTime sCreateInstanceEndTime = 0;
static PRInt32 sTimerCount = 0;
#endif /* CHEAP_PERFORMANCE_MEASURMENT */


static NS_DEFINE_IID(kITimerIID, NS_ITIMER_IID);

static nsresult FindFactory(const nsCID &  aClass,
                            nsIFactory **  aFactoryOut);

static nsresult NewTimer(const nsCID & aClass,
                         nsITimer ** aInstancePtrResult);


static nsresult NewTimer(const nsCID & aClass,
                         nsITimer ** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  
  if (nsnull == aInstancePtrResult) 
    return NS_ERROR_NULL_POINTER;
  
  nsresult   rv = NS_ERROR_FAILURE;
  nsITimer * timer = nsnull;

#ifdef CHEAP_PERFORMANCE_MEASURMENT
  sCreateInstanceStartTime = PR_Now();
#endif

#ifdef CACHE_FACTORY
  static nsIFactory * factory = nsnull;

  if (nsnull == factory)
  {
    nsresult frv = FindFactory(aClass,&factory);

    NS_ASSERTION(NS_SUCCEEDED(frv),"Could not find timer factory.");
    NS_ASSERTION(nsnull != factory,"Could not instanciate timer factory.");
  }

  if (nsnull != factory)
  {
	rv = factory->CreateInstance(NULL,
								 kITimerIID,
								 (void **)& timer);

  }
#else

  rv = nsComponentManager::CreateInstance(aClass,
                                          nsnull,
                                          kITimerIID,
                                          (void **)& timer);
#endif

#ifdef CHEAP_PERFORMANCE_MEASURMENT
  sCreateInstanceEndTime = PR_Now();
#endif

  NS_ASSERTION(NS_SUCCEEDED(rv),"Could not instanciate a timer.");


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
static nsresult FindFactory(const nsCID &  aClass,
                            nsIFactory **  aFactoryOut)
{

  NS_ASSERTION(nsnull != aFactoryOut,"NULL out pointer.");

  static nsIFactory * factory = nsnull;
  nsresult rv = NS_ERROR_FAILURE;

  *aFactoryOut = nsnull;

  if (nsnull == factory)
  {
    rv = nsComponentManager::FindFactory(aClass,&factory);
  }

  *aFactoryOut = factory;

  return rv;
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
nsresult NS_NewTimer(nsITimer ** aInstancePtrResult)
{
#ifdef CHEAP_PERFORMANCE_MEASURMENT
  sStartTime = PR_Now();
#endif

  const nsCID * cid = GetTimerCID();

  NS_ASSERTION(nsnull != cid,"Dude! Trying to make a timer with a null CID.");

  nsresult rv = NewTimer(*cid,aInstancePtrResult);

#ifdef CHEAP_PERFORMANCE_MEASURMENT
  sEndTime = PR_Now();

  printf("NS_NewTimer(count=%-4d,time=%lld ms,CI time=%lld ms)\n",
         sTimerCount++,
         sEndTime - sStartTime,
		 sCreateInstanceEndTime - sCreateInstanceStartTime);
#endif

  return rv;
}
