/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsISupportsImpl.h"
#include "mozilla/Assertions.h"

using namespace mozilla;

nsresult NS_FASTCALL
NS_TableDrivenQI(void* aThis, REFNSIID aIID, void** aInstancePtr,
                 const QITableEntry* aEntries)
{
  do {
    if (aIID.Equals(*aEntries->iid)) {
      nsISupports* r = reinterpret_cast<nsISupports*>(
        reinterpret_cast<char*>(aThis) + aEntries->offset);
      NS_ADDREF(r);
      *aInstancePtr = r;
      return NS_OK;
    }

    ++aEntries;
  } while (aEntries->iid);

  *aInstancePtr = nullptr;
  return NS_ERROR_NO_INTERFACE;
}

#ifdef MOZ_THREAD_SAFETY_OWNERSHIP_CHECKS_SUPPORTED
nsAutoOwningThread::nsAutoOwningThread()
  : mThread(GetCurrentVirtualThread())
{
}

void
nsAutoOwningThread::AssertCurrentThreadOwnsMe(const char* msg) const
{
  if (MOZ_UNLIKELY(!IsCurrentThread())) {
    // `msg` is a string literal by construction.
    MOZ_CRASH_UNSAFE_OOL(msg);
  }
}

bool
nsAutoOwningThread::IsCurrentThread() const
{
  return mThread == GetCurrentVirtualThread();
}
#endif
