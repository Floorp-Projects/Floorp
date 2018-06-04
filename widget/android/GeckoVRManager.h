/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_GeckoVRManager_h_
#define mozilla_GeckoVRManager_h_

#include "GeneratedJNINatives.h"
#include "mozilla/jni/Utils.h"

namespace mozilla {

class GeckoVRManager
{
public:
  static void* GetExternalContext() {
    return reinterpret_cast<void*>(mozilla::java::GeckoVRManager::GetExternalContext());
  }
};

} // namespace mozilla

#endif // mozilla_GeckoVRManager_h_
