/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_toolkit_system_nsWindowsPackageManager_h
#define mozilla_toolkit_system_nsWindowsPackageManager_h

#include "nsIWindowsPackageManager.h"

namespace mozilla {
namespace toolkit {
namespace system {

class nsWindowsPackageManager final : public nsIWindowsPackageManager {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIWINDOWSPACKAGEMANAGER

 private:
  ~nsWindowsPackageManager(){};
};

}  // namespace system
}  // namespace toolkit
}  // namespace mozilla

#endif  // mozilla_toolkit_system_nsWindowsPackageManager_h
