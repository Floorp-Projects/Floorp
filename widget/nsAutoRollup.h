/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsAutoRollup_h__
#define nsAutoRollup_h__

#include "mozilla/Attributes.h"     // for MOZ_RAII
#include "mozilla/StaticPtr.h"      // for StaticRefPtr

class nsIContent;

namespace mozilla {
namespace widget {

// A situation can occur when a mouse event occurs over a menu label while the
// menu popup is already open. The expected behaviour is to close the popup.
// This happens by calling nsIRollupListener::Rollup before the mouse event is
// processed. However, in cases where the mouse event is not consumed, this
// event will then get targeted at the menu label causing the menu to open
// again. To prevent this, we store in sLastRollup a reference to the popup
// that was closed during the Rollup call, and prevent this popup from
// reopening while processing the mouse event.
// sLastRollup can only be set while an nsAutoRollup is in scope;
// when it goes out of scope sLastRollup is cleared automatically.
// As sLastRollup is static, it can be retrieved by calling
// nsAutoRollup::GetLastRollup.
class MOZ_RAII nsAutoRollup
{
public:
  nsAutoRollup();
  ~nsAutoRollup();

  static void SetLastRollup(nsIContent* aLastRollup);
  // Return the popup that was last rolled up, or null if there isn't one.
  static nsIContent* GetLastRollup();

private:
  // Whether sLastRollup was clear when this nsAutoRollup
  // was created.
  bool mWasClear;

  // The number of nsAutoRollup instances active.
  static uint32_t sCount;
  // The last rolled up popup.
  static StaticRefPtr<nsIContent> sLastRollup;
};

} // namespace widget
} // namespace mozilla

#endif // nsAutoRollup_h__
