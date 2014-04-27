/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(ACCESSIBILITY)

#include "UIAAccessibilityBridge.h"
#include "MetroUtils.h"

#include <OAIdl.h>

#include "nsIAccessibleEvent.h"
#include "nsIAccessibleEditableText.h"
#include "nsIPersistentProperties2.h"

// generated
#include "UIABridge.h"

#include <wrl/implements.h>

using namespace mozilla::a11y;

namespace mozilla {
namespace widget {
namespace winrt {

using namespace Microsoft::WRL;

NS_IMPL_ISUPPORTS(AccessibilityBridge, nsIObserver)

nsresult
AccessibilityBridge::Observe(nsISupports *aSubject, const char *aTopic, const char16_t *aData)
{
  nsCOMPtr<nsIAccessibleEvent> ev = do_QueryInterface(aSubject);
  if (!ev) {
    return NS_OK;
  }

  uint32_t eventType = 0;
  ev->GetEventType(&eventType);
  if (eventType == nsIAccessibleEvent::EVENT_FOCUS) {
    Microsoft::WRL::ComPtr<IUIABridge> bridgePtr;
    mBridge.As(&bridgePtr);
    if (bridgePtr) {
      bridgePtr->FocusChangeEvent();
    }
  }

  return NS_OK;
}

} } }

#endif // ACCESSIBILITY
