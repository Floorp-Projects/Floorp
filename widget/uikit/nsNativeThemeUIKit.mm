/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeThemeUIKit.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::widget;

nsNativeThemeUIKit::nsNativeThemeUIKit() : Theme(ScrollbarStyle()) {}

already_AddRefed<Theme> do_CreateNativeThemeDoNotUseDirectly() {
  if (gfxPlatform::IsHeadless()) {
    return do_AddRef(new Theme(Theme::ScrollbarStyle()));
  }
  return do_AddRef(new nsNativeThemeUIKit());
}
