/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ColorScheme_h
#define mozilla_ColorScheme_h

#include <cstdint>

namespace mozilla {

// Whether we should use a light or dark appearance.
enum class ColorScheme : uint8_t { Light, Dark };

// The color-scheme we should get back in case it's not explicit. The "used"
// mode is the mode that most of layout should use (defaults to light if the
// page doesn't have a color-scheme property / meta-tag indicating otherwise).
//
// The "preferred" mode returns the color-scheme for purposes of
// prefers-color-scheme propagation (that is, it defaults to the current
// preferred color-scheme, rather than light).
enum class ColorSchemeMode : uint8_t {
  Used,
  Preferred,
};

}  // namespace mozilla

#endif
