/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_NativeMenuSupport_h
#define mozilla_widget_NativeMenuSupport_h

class nsIWidget;

namespace mozilla {

namespace dom {
class Element;
}

namespace widget {

class NativeMenuSupport final {
 public:
  // Given a top-level window widget and a menu bar DOM node, sets up native
  // menus. Once created, native menus are controlled via the DOM, including
  // destruction.
  static void CreateNativeMenuBar(nsIWidget* aParent,
                                  dom::Element* aMenuBarElement);
};

}  // namespace widget
}  // namespace mozilla

#endif  // mozilla_widget_NativeMenuSupport_h
