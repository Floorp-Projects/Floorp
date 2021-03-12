/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ScrollbarUtil_h
#define ScrollbarUtil_h

#include "nsITheme.h"
#include "nsNativeTheme.h"

class ScrollbarUtil {
 public:
  static bool IsScrollbarWidthThin(mozilla::ComputedStyle* aStyle);
  static bool IsScrollbarWidthThin(nsIFrame* aFrame);

  // Returns the style for custom scrollbar if the scrollbar part frame should
  // use the custom drawing path, nullptr otherwise.
  //
  // Optionally the caller can pass a pointer to aDarkScrollbar for whether
  // custom scrollbar may be drawn due to dark background.
  static mozilla::ComputedStyle* GetCustomScrollbarStyle(
      nsIFrame* aFrame, bool* aDarkScrollbar = nullptr);

  static nscolor GetScrollbarTrackColor(nsIFrame* aFrame);
  static nscolor GetScrollbarThumbColor(nsIFrame* aFrame,
                                        mozilla::EventStates aEventStates);
  static mozilla::Maybe<nsITheme::Transparency> GetScrollbarPartTransparency(
      nsIFrame* aFrame, mozilla::StyleAppearance aAppearance);

 protected:
  ScrollbarUtil() = default;
  virtual ~ScrollbarUtil() = default;
};

#endif
