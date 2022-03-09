
/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_NativeMenuGtk_h
#define mozilla_widget_NativeMenuGtk_h

#include "mozilla/widget/NativeMenu.h"
#include "GRefPtr.h"

namespace mozilla {

namespace dom {
class Element;
}

namespace widget {

class MenuModel;

class NativeMenuGtk : public NativeMenu {
 public:
  // Whether we can use native menu popups on GTK.
  static bool CanUse();

  explicit NativeMenuGtk(dom::Element* aElement);

  // NativeMenu
  void ShowAsContextMenu(nsPresContext*, const CSSIntPoint& aPosition) override;
  bool Close() override;
  void ActivateItem(dom::Element* aItemElement, Modifiers aModifiers,
                    int16_t aButton, ErrorResult& aRv) override;
  void OpenSubmenu(dom::Element* aMenuElement) override;
  void CloseSubmenu(dom::Element* aMenuElement) override;
  RefPtr<dom::Element> Element() override;
  void AddObserver(NativeMenu::Observer* aObserver) override {
    mObservers.AppendElement(aObserver);
  }
  void RemoveObserver(NativeMenu::Observer* aObserver) override {
    mObservers.RemoveElement(aObserver);
  }

  void Dump();

  void OnUnmap();

 protected:
  virtual ~NativeMenuGtk();

  bool mPoppedUp = false;
  RefPtr<GtkWidget> mNativeMenu;
  RefPtr<MenuModel> mMenuModel;
  nsTArray<NativeMenu::Observer*> mObservers;
};

}  // namespace widget
}  // namespace mozilla

#endif
