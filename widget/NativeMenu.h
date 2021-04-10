/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_NativeMenu_h
#define mozilla_widget_NativeMenu_h

#include "nsISupportsImpl.h"
#include "Units.h"

namespace mozilla::dom {
class Element;
}

namespace mozilla::widget {

class NativeMenu {
 public:
  NS_INLINE_DECL_REFCOUNTING(NativeMenu)

  // Show this menu as a context menu at the specified position.
  // This call assumes that the popupshowing event for the root popup has
  // already been sent and "approved", i.e. preventDefault() was not called.
  virtual void ShowAsContextMenu(const mozilla::DesktopPoint& aPosition) = 0;

  // Close the menu and synchronously fire popuphiding / popuphidden events.
  // Returns false if the menu wasn't open.
  virtual bool Close() = 0;

  // Return this NativeMenu's DOM element.
  virtual RefPtr<dom::Element> Element() = 0;

  class Observer {
   public:
    // Called when the menu opened, after popupshown.
    // No strong reference is held to the observer during the call.
    virtual void OnNativeMenuOpened() = 0;

    // Called when the menu closed, after popuphidden.
    // No strong reference is held to the observer during the call.
    virtual void OnNativeMenuClosed() = 0;
  };

  // Add an observer that gets notified of menu opening and closing.
  // The menu does not keep a strong reference the observer. The observer must
  // remove itself before it is destroyed.
  virtual void AddObserver(Observer* aObserver) = 0;

  // Remove an observer that was previously added with AddObserver.
  virtual void RemoveObserver(Observer* aObserver) = 0;

 protected:
  virtual ~NativeMenu() = default;
};

}  // namespace mozilla::widget

#endif  // mozilla_widget_NativeMenu_h
