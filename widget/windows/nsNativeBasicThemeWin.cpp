/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeBasicThemeWin.h"

#include "mozilla/ClearOnShutdown.h"
#include "ScrollbarUtil.h"

nsITheme::Transparency nsNativeBasicThemeWin::GetWidgetTransparency(
    nsIFrame* aFrame, StyleAppearance aAppearance) {
  if (auto transparency =
          ScrollbarUtil::GetScrollbarPartTransparency(aFrame, aAppearance)) {
    return *transparency;
  }
  return nsNativeBasicTheme::GetWidgetTransparency(aFrame, aAppearance);
}

already_AddRefed<nsITheme> do_GetBasicNativeThemeDoNotUseDirectly() {
  static mozilla::StaticRefPtr<nsITheme> gInstance;
  if (MOZ_UNLIKELY(!gInstance)) {
    gInstance = new nsNativeBasicThemeWin();
    ClearOnShutdown(&gInstance);
  }
  return do_AddRef(gInstance);
}
