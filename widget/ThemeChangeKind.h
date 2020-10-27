/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_ThemeChangeKind
#define mozilla_widget_ThemeChangeKind

#include "mozilla/TypedEnumBits.h"

namespace mozilla::widget {

enum class ThemeChangeKind : uint8_t {
  // This is the cheapest change, no need to forcibly recompute style and/or
  // layout.
  MediaQueriesOnly = 0,
  // Style needs to forcibly be recomputed because some of the stuff that may
  // have changed, like system colors, are reflected in the computed style but
  // not in the specified style.
  Style = 1 << 0,
  // Layout needs to forcibly be recomputed because some of the stuff that may
  // have changed is layout-dependent, like system font.
  Layout = 1 << 1,
  // The union of the two flags above.
  StyleAndLayout = Style | Layout,
  // For IPC serialization purposes.
  AllBits = Style | Layout,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(ThemeChangeKind)

}  // namespace mozilla::widget

#endif
