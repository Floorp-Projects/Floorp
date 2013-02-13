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
public:
  static PRLock *getDebugFDsLock() {
    // We have to use something lower level than a mutex. If we don't, we
    // can get recursive in here when called from logging a call to free.
    static PRLock *Lock = PR_NewLock();
    return Lock;
  }

  DebugFDAutoLock() : Scoped<DebugFDAutoLockTraits>(getDebugFDsLock()) {
    PR_Lock(get());
  }
};

bool PoisonWriteEnabled();
void PoisonWriteBase();
bool ValidWriteAssert(bool ok);
void BaseCleanup();
// This method should always be called with the debugFDs lock.
// Note: MSVC Local static is NOT thread safe
// http://stackoverflow.com/questions/10585928/is-static-init-thread-safe-with-vc2010
std::vector<int>& getDebugFDs();

}

#endif
