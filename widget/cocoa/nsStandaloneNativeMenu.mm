/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>

#include "nsStandaloneNativeMenu.h"

#include "mozilla/dom/Element.h"
#include "NativeMenuMac.h"
#include "nsISupports.h"
#include "nsGkAtoms.h"

using namespace mozilla;

using mozilla::dom::Element;

NS_IMPL_ISUPPORTS(nsStandaloneNativeMenu, nsIStandaloneNativeMenu)

nsStandaloneNativeMenu::nsStandaloneNativeMenu() = default;

nsStandaloneNativeMenu::~nsStandaloneNativeMenu() = default;

NS_IMETHODIMP
nsStandaloneNativeMenu::Init(Element* aElement) {
  NS_ASSERTION(mMenu == nullptr, "nsNativeMenu::Init - mMenu not null!");

  NS_ENSURE_ARG(aElement);

  if (!aElement->IsAnyOfXULElements(nsGkAtoms::menu, nsGkAtoms::menupopup)) {
    return NS_ERROR_FAILURE;
  }

  mMenu = new mozilla::widget::NativeMenuMac(aElement);

  return NS_OK;
}

NS_IMETHODIMP
nsStandaloneNativeMenu::MenuWillOpen(bool* aResult) {
  NS_ASSERTION(mMenu != nullptr,
               "nsStandaloneNativeMenu::OnOpen - mMenu is null!");

  mMenu->MenuWillOpen();

  *aResult = true;
  return NS_OK;
}

NS_IMETHODIMP
nsStandaloneNativeMenu::ActivateNativeMenuItemAt(
    const nsAString& aIndexString) {
  if (!mMenu) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (mMenu->ActivateNativeMenuItemAt(aIndexString)) {
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsStandaloneNativeMenu::ForceUpdateNativeMenuAt(const nsAString& aIndexString) {
  if (!mMenu) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  mMenu->ForceUpdateNativeMenuAt(aIndexString);

  return NS_OK;
}

NS_IMETHODIMP
nsStandaloneNativeMenu::Dump() {
  mMenu->Dump();

  return NS_OK;
}
