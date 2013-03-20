/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCycleCollectorUtils_h__
#define nsCycleCollectorUtils_h__

#include "nscore.h"
#include "mozilla/threads/nsThreadIDs.h"

#if defined(MOZILLA_INTERNAL_API)
#if defined(XP_WIN)
bool NS_IsCycleCollectorThread();
#elif defined(NS_TLS)
// Defined in nsThreadManager.cpp.
extern NS_TLS mozilla::threads::ID gTLSThreadID;
inline bool NS_IsCycleCollectorThread()
{
  return gTLSThreadID == mozilla::threads::CycleCollector;
}
#else
NS_COM_GLUE bool NS_IsCycleCollectorThread();
#endif
#else
NS_COM_GLUE bool NS_IsCycleCollectorThread();
#endif

#endif /* nsCycleCollectorUtils_h__ */
