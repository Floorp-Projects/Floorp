/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNativeBasicThemeWin_h
#define nsNativeBasicThemeWin_h

#include "nsNativeBasicTheme.h"

class nsNativeBasicThemeWin : public nsNativeBasicTheme {
 public:
  nsNativeBasicThemeWin() = default;

  Transparency GetWidgetTransparency(nsIFrame* aFrame,
                                     StyleAppearance aAppearance) override;

 protected:
  virtual ~nsNativeBasicThemeWin() = default;

  std::pair<sRGBColor, sRGBColor> ComputeCheckboxColors(
      const EventStates& aState, StyleAppearance aAppearance) override;
  sRGBColor ComputeCheckmarkColor(const EventStates& aState) override;
  sRGBColor ComputeBorderColor(const EventStates& aState) override;
  std::pair<sRGBColor, sRGBColor> ComputeButtonColors(
      const EventStates& aState, nsIFrame* aFrame = nullptr) override;
  std::pair<sRGBColor, sRGBColor> ComputeTextfieldColors(
      const EventStates& aState) override;
  std::pair<sRGBColor, sRGBColor> ComputeRangeProgressColors(
      const EventStates& aState) override;
  std::pair<sRGBColor, sRGBColor> ComputeRangeTrackColors(
      const EventStates& aState) override;
  std::pair<sRGBColor, sRGBColor> ComputeRangeThumbColors(
      const EventStates& aState) override;
  std::pair<sRGBColor, sRGBColor> ComputeProgressColors() override;
  std::pair<sRGBColor, sRGBColor> ComputeProgressTrackColors() override;
  std::pair<sRGBColor, sRGBColor> ComputeMeterchunkColors(
      const EventStates& aMeterState) override;
  std::pair<sRGBColor, sRGBColor> ComputeMeterTrackColors() override;
  sRGBColor ComputeMenulistArrowButtonColor(const EventStates& aState) override;
  std::array<sRGBColor, 3> ComputeFocusRectColors() override;
  std::pair<sRGBColor, sRGBColor> ComputeScrollbarColors(
      nsIFrame* aFrame, const ComputedStyle& aStyle,
      const EventStates& aDocumentState) override;
  sRGBColor ComputeScrollbarThumbColor(
      nsIFrame* aFrame, const ComputedStyle& aStyle,
      const EventStates& aElementState,
      const EventStates& aDocumentState) override;
  std::array<sRGBColor, 3> ComputeScrollbarButtonColors(
      nsIFrame* aFrame, StyleAppearance aAppearance,
      const ComputedStyle& aStyle, const EventStates& aElementState,
      const EventStates& aDocumentState) override;
};

#endif
