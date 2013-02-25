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

//#define DEBUG_BRIDGE
#if !defined(DEBUG_BRIDGE)
#undef LogThread
#undef LogFunction
#undef Log
#define LogThread() 
#define LogFunction()
#define Log(...)
#endif

using namespace mozilla::a11y;

namespace mozilla {
namespace widget {
namespace winrt {

using namespace Microsoft::WRL;

NS_IMPL_ISUPPORTS1(AccessibilityBridge, nsIObserver)

nsresult
AccessibilityBridge::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *aData)
{
  nsCOMPtr<nsIAccessibleEvent> ev = do_QueryInterface(aSubject);
  if (!ev) {
    return NS_OK;
  }

  uint32_t eventType = 0;
  ev->GetEventType(&eventType);

  switch (eventType) {
    case nsIAccessibleEvent::EVENT_FOCUS:
      Log(L"EVENT_FOCUS");
      {
        nsCOMPtr<nsIAccessible> item;
        ev->GetAccessible(getter_AddRefs(item));
        Accessible* access = (Accessible*)item.get();
        Log(L"Focus element flags: %d %d %d",
          ((access->NativeState() & mozilla::a11y::states::EDITABLE) > 0),
          ((access->NativeState() & mozilla::a11y::states::FOCUSABLE) > 0),
          ((access->NativeState() & mozilla::a11y::states::READONLY) > 0)
          );
        bool focusable = (((access->NativeState() & mozilla::a11y::states::EDITABLE) > 0) &&
                          ((access->NativeState() & mozilla::a11y::states::READONLY) == 0));

        if (focusable) {
          Log(L"focus item can be focused");
          // we have a text input
          Microsoft::WRL::ComPtr<IUIAElement> bridgePtr;
          mBridge.As(&bridgePtr);
          if (bridgePtr) {
            bridgePtr->SetFocusInternal(reinterpret_cast<LONG_PTR>(item.get()));
          }
        } else {
          Log(L"focus item can't have focus");
          Microsoft::WRL::ComPtr<IUIAElement> bridgePtr;
          mBridge.As(&bridgePtr);
          if (bridgePtr) {
            bridgePtr->ClearFocus();
          }
        }
      }
    break;
    /*
    case nsIAccessibleEvent::EVENT_SHOW:
      Log(L"EVENT_SHOW");
    break;
    case nsIAccessibleEvent::EVENT_HIDE:
      Log(L"EVENT_HIDE");
    break;
    case nsIAccessibleEvent::EVENT_STATE_CHANGE:
      Log(L"EVENT_STATE_CHANGE");
    break;
    case nsIAccessibleEvent::EVENT_SELECTION:
      Log(L"EVENT_SELECTION");
    break;
    case nsIAccessibleEvent::EVENT_SELECTION_ADD:
      Log(L"EVENT_SELECTION_ADD");
    break;
    case nsIAccessibleEvent::EVENT_SELECTION_REMOVE:
      Log(L"EVENT_SELECTION_REMOVE");
    break;
    case nsIAccessibleEvent::EVENT_SELECTION_WITHIN:
      Log(L"EVENT_SELECTION_WITHIN");
    break;
    case nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED:
      Log(L"EVENT_TEXT_CARET_MOVED");
    break;
    case nsIAccessibleEvent::EVENT_TEXT_CHANGED:
      Log(L"EVENT_TEXT_CHANGED");
    break;
    case nsIAccessibleEvent::EVENT_TEXT_INSERTED:
      Log(L"EVENT_TEXT_INSERTED");
    break;
    case nsIAccessibleEvent::EVENT_TEXT_REMOVED:
      Log(L"EVENT_TEXT_REMOVED");
    break;
    case nsIAccessibleEvent::EVENT_TEXT_UPDATED:
      Log(L"EVENT_TEXT_UPDATED");
    break;
    case nsIAccessibleEvent::EVENT_TEXT_SELECTION_CHANGED:
      Log(L"EVENT_TEXT_SELECTION_CHANGED");
    break;
    case nsIAccessibleEvent::EVENT_SECTION_CHANGED:
      Log(L"EVENT_SECTION_CHANGED");
    break;
    case nsIAccessibleEvent::EVENT_WINDOW_ACTIVATE:
      Log(L"EVENT_WINDOW_ACTIVATE");
    break;
    */
    default:
    return NS_OK;
  }

  return NS_OK;
}

} } }

#endif // ACCESSIBILITY
