/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozwrlbase.h"

#include "nscore.h"
#include "nsString.h"
#include "nsIDOMWakeLockListener.h"

#include <windows.system.display.h>

/*
 * A wake lock is used by dom to prevent the device from turning off the
 * screen when the user is viewing certain types of content, like video.
 */
class WakeLockListener :
  public nsIDOMMozWakeLockListener {
public:
  NS_DECL_ISUPPORTS;
  NS_DECL_NSIDOMMOZWAKELOCKLISTENER;

private:
  Microsoft::WRL::ComPtr<ABI::Windows::System::Display::IDisplayRequest> mDisplayRequest;
};
