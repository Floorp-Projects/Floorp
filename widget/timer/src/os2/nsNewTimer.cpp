/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 *  Michael Lowe <michael.lowe@bigfoot.com>
 */
#include "nsITimer.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"

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

#ifdef CACHE_FACTORY
  static nsIFactory* factory = nsnull;

  if (nsnull == factory) {
    nsresult frv = FindFactory(aClass,&factory);

    NS_ASSERTION(NS_SUCCEEDED(frv),"Could not find timer factory.");
    NS_ASSERTION(nsnull != factory,"Could not instanciate timer factory.");
  }

  if (nsnull != factory) {
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

  NS_ASSERTION(NS_SUCCEEDED(rv),"Could not instanciate a timer.");


  if (nsnull == timer) 
    return NS_ERROR_OUT_OF_MEMORY;

  *aInstancePtrResult = timer;
  
  return rv;

// Dont QI() cause the factory above will do it for us.  Otherwise
// the refcnt will be one too much and every single timer will leak.

//  return timer->QueryInterface(kITimerIID, (void **) aInstancePtrResult);
}
//////////////////////////////////////////////////////////////////////////
static nsresult FindFactory(const nsCID &  aClass,
                            nsIFactory **  aFactoryOut)
{

  NS_ASSERTION(nsnull != aFactoryOut, "NULL out pointer.");

  static nsIFactory * factory = nsnull;
  nsresult rv = NS_ERROR_FAILURE;

  *aFactoryOut = nsnull;

  if (nsnull == factory) {
    rv = nsComponentManager::FindFactory(aClass,&factory);
  }

  *aFactoryOut = factory;

  return rv;
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
nsresult NS_NewTimer(nsITimer ** aInstancePtrResult)
{
  static NS_DEFINE_CID(kCTIMER_CID, NS_TIMER_CID);
  nsresult rv = NewTimer(kCTIMER_CID, aInstancePtrResult);

  return rv;
}
