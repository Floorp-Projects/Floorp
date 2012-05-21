/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCycleCollectorUtils.h"

#include "prthread.h"
#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsXPCOMCIDInternal.h"

#include "nsIThreadManager.h"

#if defined(XP_WIN)
#include "windows.h"
#endif

#ifndef MOZILLA_INTERNAL_API

bool
NS_IsCycleCollectorThread()
{
  bool result = false;
  nsCOMPtr<nsIThreadManager> mgr =
    do_GetService(NS_THREADMANAGER_CONTRACTID);
  if (mgr)
    mgr->GetIsCycleCollectorThread(&result);
  return bool(result);
}

#elif defined(XP_WIN)

// Defined in nsThreadManager.cpp.
extern DWORD gTLSThreadIDIndex;

bool
NS_IsCycleCollectorThread()
{
  return TlsGetValue(gTLSThreadIDIndex) ==
    (void*) mozilla::threads::CycleCollector;
}

#elif !defined(NS_TLS)

// Defined in nsCycleCollector.cpp
extern PRThread* gCycleCollectorThread;

bool
NS_IsCycleCollectorThread()
{
  return PR_GetCurrentThread() == gCycleCollectorThread;
}

#endif
