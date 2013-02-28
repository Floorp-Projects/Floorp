/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 ci et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Private interface for code shared between the platforms of mozPoisonWriteXXX

#ifndef MOZPOISONWRITEBASE_H
#define MOZPOISONWRITEBASE_H

#include <stdio.h>
#include <vector>
#include "nspr.h"
#include "mozilla/NullPtr.h"
#include "mozilla/Util.h"
#include "mozilla/Scoped.h"

namespace mozilla {


struct DebugFDAutoLockTraits {
  typedef PRLock *type;
  const static type empty() {
    return nullptr;
  }
  const static void release(type aL) {
    PR_Unlock(aL);
  }
};

class DebugFDAutoLock : public Scoped<DebugFDAutoLockTraits> {
  static PRLock *Lock;
public:
  static void Clear();
  static PRLock *getDebugFDsLock() {
    // On windows this static is not thread safe, but we know that the first
    // call is from
    // * An early registration of a debug FD or
    // * The call to InitWritePoisoning.
    // Since the early debug FDs are logs created early in the main thread
    // and no writes are trapped before InitWritePoisoning, we are safe.
    static bool Initialized = false;
    if (!Initialized) {
      Lock = PR_NewLock();
      Initialized = true;
    }

    // We have to use something lower level than a mutex. If we don't, we
    // can get recursive in here when called from logging a call to free.
    return Lock;
  }

  DebugFDAutoLock() : Scoped<DebugFDAutoLockTraits>(getDebugFDsLock()) {
    PR_Lock(get());
  }
};

bool PoisonWriteEnabled();
bool ValidWriteAssert(bool ok);
void BaseCleanup();
// This method should always be called with the debugFDs lock.
// Note: MSVC Local static is NOT thread safe
// http://stackoverflow.com/questions/10585928/is-static-init-thread-safe-with-vc2010
std::vector<int>& getDebugFDs();

}

#endif
