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

static NS_DEFINE_IID(kITimerIID, NS_ITIMER_IID);
static NS_DEFINE_CID(kCTimerGtkCID, NS_TIMER_GTK_CID);

//////////////////////////////////////////////////////////////////////////
nsresult NS_NewTimer(nsITimer ** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  
  if (nsnull == aInstancePtrResult) 
    return NS_ERROR_NULL_POINTER;
  
  nsresult   rv = NS_ERROR_FAILURE;

  nsITimer * timer = nsnull;
  
  static nsIFactory * factory = nsnull;

  if (nsnull == factory)
  {
    nsresult frv = nsComponentManager::FindFactory(kCTimerGtkCID,&factory);

    NS_ASSERTION(NS_SUCCEEDED(frv),"Could not find timer factory.");
    NS_ASSERTION(nsnull != factory,"Could not instanciate timer factory.");

    if (NS_FAILED(frv))
      return frv;
  }

  rv = factory->CreateInstance(NULL,
                               kITimerIID,
                               (void **)& timer);

  NS_ASSERTION(NS_SUCCEEDED(rv),"Could not instanciate a timer.");

  if (nsnull == timer) 
    return NS_ERROR_OUT_OF_MEMORY;

  *aInstancePtrResult = timer;
  
  return rv;
}
//////////////////////////////////////////////////////////////////////////
