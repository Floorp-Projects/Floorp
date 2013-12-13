/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef InjectCrashReporter_h
#define InjectCrashReporter_h

#include "nsThreadUtils.h"
#include <windows.h>

namespace mozilla {

class InjectCrashRunnable : public nsRunnable
{
public:
  InjectCrashRunnable(DWORD pid);

  NS_IMETHOD Run();

private:
  DWORD mPID;
  nsString mInjectorPath;
};
  
} // Namespace mozilla

#endif
