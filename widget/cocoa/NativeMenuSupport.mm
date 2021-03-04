/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/widget/NativeMenuSupport.h"

#include "MainThreadUtils.h"
#include "nsCocoaWindow.h"
#include "nsMenuBarX.h"

namespace mozilla::widget {

void NativeMenuSupport::CreateNativeMenuBar(nsIWidget* aParent, dom::Element* aMenuBarElement) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread(), "Attempting to create native menu bar on wrong thread!");

  RefPtr<nsMenuBarX> mb = new nsMenuBarX();

  nsresult rv = mb->Create(aMenuBarElement);
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));

  // Give the menubar to the parent window. The parent takes ownership.
  static_cast<nsCocoaWindow*>(aParent)->SetMenuBar(std::move(mb));
}

}  // namespace mozilla::widget
