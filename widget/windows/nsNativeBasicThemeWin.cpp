/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeBasicThemeWin.h"

#include "LookAndFeel.h"
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

std::pair<sRGBColor, sRGBColor> nsNativeBasicThemeWin::ComputeCheckboxColors(
    const EventStates& aState, StyleAppearance aAppearance) {
  if (!LookAndFeel::GetInt(LookAndFeel::IntID::UseAccessibilityTheme, 0)) {
    return nsNativeBasicTheme::ComputeCheckboxColors(aState, aAppearance);
  }

  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);
  bool isChecked = aState.HasState(NS_EVENT_STATE_CHECKED);
  bool isIndeterminate = aAppearance == StyleAppearance::Checkbox &&
                         aState.HasState(NS_EVENT_STATE_INDETERMINATE);

  sRGBColor backgroundColor = sRGBColor::FromABGR(
      LookAndFeel::GetColor(LookAndFeel::ColorID::TextBackground));
  sRGBColor borderColor = sRGBColor::FromABGR(
      LookAndFeel::GetColor(LookAndFeel::ColorID::Buttontext));
  if (isDisabled && (isChecked || isIndeterminate)) {
    backgroundColor = borderColor = sRGBColor::FromABGR(
        LookAndFeel::GetColor(LookAndFeel::ColorID::Graytext));
  } else if (isDisabled) {
    borderColor = sRGBColor::FromABGR(
        LookAndFeel::GetColor(LookAndFeel::ColorID::Graytext));
  } else if (isChecked || isIndeterminate) {
    backgroundColor = borderColor = sRGBColor::FromABGR(
        LookAndFeel::GetColor(LookAndFeel::ColorID::Highlight));
  }

  return std::make_pair(backgroundColor, borderColor);
}

sRGBColor nsNativeBasicThemeWin::ComputeCheckmarkColor(
    const EventStates& aState) {
  if (!LookAndFeel::GetInt(LookAndFeel::IntID::UseAccessibilityTheme, 0)) {
    return nsNativeBasicTheme::ComputeCheckmarkColor(aState);
  }

  return sRGBColor::FromABGR(
      LookAndFeel::GetColor(LookAndFeel::ColorID::TextBackground));
}

sRGBColor nsNativeBasicThemeWin::ComputeBorderColor(const EventStates& aState) {
  if (!LookAndFeel::GetInt(LookAndFeel::IntID::UseAccessibilityTheme, 0)) {
    return nsNativeBasicTheme::ComputeBorderColor(aState);
  }

  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);

  if (isDisabled) {
    return sRGBColor::FromABGR(
        LookAndFeel::GetColor(LookAndFeel::ColorID::Graytext));
  }
  return sRGBColor::FromABGR(
      LookAndFeel::GetColor(LookAndFeel::ColorID::Buttontext));
}

std::pair<sRGBColor, sRGBColor> nsNativeBasicThemeWin::ComputeButtonColors(
    const EventStates& aState, nsIFrame* aFrame) {
  if (!LookAndFeel::GetInt(LookAndFeel::IntID::UseAccessibilityTheme, 0)) {
    return nsNativeBasicTheme::ComputeButtonColors(aState, aFrame);
  }

  return std::make_pair(sRGBColor::FromABGR(LookAndFeel::GetColor(
                            LookAndFeel::ColorID::Buttonface)),
                        ComputeBorderColor(aState));
}

std::pair<sRGBColor, sRGBColor> nsNativeBasicThemeWin::ComputeTextfieldColors(
    const EventStates& aState) {
  if (!LookAndFeel::GetInt(LookAndFeel::IntID::UseAccessibilityTheme, 0)) {
    return nsNativeBasicTheme::ComputeTextfieldColors(aState);
  }

  return std::make_pair(sRGBColor::FromABGR(LookAndFeel::GetColor(
                            LookAndFeel::ColorID::TextBackground)),
                        ComputeBorderColor(aState));
}

std::pair<sRGBColor, sRGBColor>
nsNativeBasicThemeWin::ComputeRangeProgressColors(const EventStates& aState) {
  if (!LookAndFeel::GetInt(LookAndFeel::IntID::UseAccessibilityTheme, 0)) {
    return nsNativeBasicTheme::ComputeRangeProgressColors(aState);
  }

  return std::make_pair(sRGBColor::FromABGR(LookAndFeel::GetColor(
                            LookAndFeel::ColorID::Highlight)),
                        sRGBColor::FromABGR(LookAndFeel::GetColor(
                            LookAndFeel::ColorID::Buttontext)));
}

std::pair<sRGBColor, sRGBColor> nsNativeBasicThemeWin::ComputeRangeTrackColors(
    const EventStates& aState) {
  if (!LookAndFeel::GetInt(LookAndFeel::IntID::UseAccessibilityTheme, 0)) {
    return nsNativeBasicTheme::ComputeRangeTrackColors(aState);
  }

  return std::make_pair(sRGBColor::FromABGR(LookAndFeel::GetColor(
                            LookAndFeel::ColorID::TextBackground)),
                        sRGBColor::FromABGR(LookAndFeel::GetColor(
                            LookAndFeel::ColorID::Buttontext)));
}

std::pair<sRGBColor, sRGBColor> nsNativeBasicThemeWin::ComputeRangeThumbColors(
    const EventStates& aState) {
  if (!LookAndFeel::GetInt(LookAndFeel::IntID::UseAccessibilityTheme, 0)) {
    return nsNativeBasicTheme::ComputeRangeThumbColors(aState);
  }

  return std::make_pair(sRGBColor::FromABGR(LookAndFeel::GetColor(
                            LookAndFeel::ColorID::Highlight)),
                        sRGBColor::FromABGR(LookAndFeel::GetColor(
                            LookAndFeel::ColorID::Highlight)));
}

std::pair<sRGBColor, sRGBColor> nsNativeBasicThemeWin::ComputeProgressColors() {
  if (!LookAndFeel::GetInt(LookAndFeel::IntID::UseAccessibilityTheme, 0)) {
    return nsNativeBasicTheme::ComputeProgressColors();
  }

  return std::make_pair(sRGBColor::FromABGR(LookAndFeel::GetColor(
                            LookAndFeel::ColorID::Highlight)),
                        sRGBColor::FromABGR(LookAndFeel::GetColor(
                            LookAndFeel::ColorID::Buttontext)));
}

std::pair<sRGBColor, sRGBColor>
nsNativeBasicThemeWin::ComputeProgressTrackColors() {
  if (!LookAndFeel::GetInt(LookAndFeel::IntID::UseAccessibilityTheme, 0)) {
    return nsNativeBasicTheme::ComputeProgressTrackColors();
  }

  return std::make_pair(sRGBColor::FromABGR(LookAndFeel::GetColor(
                            LookAndFeel::ColorID::TextBackground)),
                        sRGBColor::FromABGR(LookAndFeel::GetColor(
                            LookAndFeel::ColorID::Buttontext)));
}

std::pair<sRGBColor, sRGBColor> nsNativeBasicThemeWin::ComputeMeterchunkColors(
    const EventStates& aMeterState) {
  if (!LookAndFeel::GetInt(LookAndFeel::IntID::UseAccessibilityTheme, 0)) {
    return nsNativeBasicTheme::ComputeMeterchunkColors(aMeterState);
  }

  return std::make_pair(sRGBColor::FromABGR(LookAndFeel::GetColor(
                            LookAndFeel::ColorID::Highlight)),
                        sRGBColor::FromABGR(LookAndFeel::GetColor(
                            LookAndFeel::ColorID::TextForeground)));
}

std::pair<sRGBColor, sRGBColor>
nsNativeBasicThemeWin::ComputeMeterTrackColors() {
  if (!LookAndFeel::GetInt(LookAndFeel::IntID::UseAccessibilityTheme, 0)) {
    return nsNativeBasicTheme::ComputeMeterTrackColors();
  }

  return std::make_pair(sRGBColor::FromABGR(LookAndFeel::GetColor(
                            LookAndFeel::ColorID::TextBackground)),
                        sRGBColor::FromABGR(LookAndFeel::GetColor(
                            LookAndFeel::ColorID::TextForeground)));
}

sRGBColor nsNativeBasicThemeWin::ComputeMenulistArrowButtonColor(
    const EventStates& aState) {
  if (!LookAndFeel::GetInt(LookAndFeel::IntID::UseAccessibilityTheme, 0)) {
    return nsNativeBasicTheme::ComputeMenulistArrowButtonColor(aState);
  }

  bool isDisabled = aState.HasState(NS_EVENT_STATE_DISABLED);

  if (isDisabled) {
    return sRGBColor::FromABGR(
        LookAndFeel::GetColor(LookAndFeel::ColorID::Graytext));
  }
  return sRGBColor::FromABGR(
      LookAndFeel::GetColor(LookAndFeel::ColorID::TextForeground));
}

std::array<sRGBColor, 3> nsNativeBasicThemeWin::ComputeFocusRectColors() {
  if (!LookAndFeel::GetInt(LookAndFeel::IntID::UseAccessibilityTheme, 0)) {
    return nsNativeBasicTheme::ComputeFocusRectColors();
  }

  return {sRGBColor::FromABGR(
              LookAndFeel::GetColor(LookAndFeel::ColorID::Highlight)),
          sRGBColor::FromABGR(
              LookAndFeel::GetColor(LookAndFeel::ColorID::Buttontext)),
          sRGBColor::FromABGR(
              LookAndFeel::GetColor(LookAndFeel::ColorID::TextBackground))};
}

std::pair<sRGBColor, sRGBColor> nsNativeBasicThemeWin::ComputeScrollbarColors(
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aDocumentState) {
  if (!LookAndFeel::GetInt(LookAndFeel::IntID::UseAccessibilityTheme, 0)) {
    nscolor trackColor = ScrollbarUtil::GetScrollbarTrackColor(aFrame);
    sRGBColor color = sRGBColor::FromABGR(trackColor);
    return std::make_pair(color, color);
  }

  sRGBColor color = sRGBColor::FromABGR(
      LookAndFeel::GetColor(LookAndFeel::ColorID::TextBackground));
  return std::make_pair(color, color);
}

sRGBColor nsNativeBasicThemeWin::ComputeScrollbarThumbColor(
    nsIFrame* aFrame, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState) {
  if (!LookAndFeel::GetInt(LookAndFeel::IntID::UseAccessibilityTheme, 0)) {
    return gfx::sRGBColor::FromABGR(
        ScrollbarUtil::GetScrollbarThumbColor(aFrame, aElementState));
  }

  bool isActive = aElementState.HasState(NS_EVENT_STATE_ACTIVE);
  bool isHovered = aElementState.HasState(NS_EVENT_STATE_HOVER);
  const nsStyleUI* ui = aStyle.StyleUI();
  nscolor color;

  if (ui->mScrollbarColor.IsColors()) {
    color = ui->mScrollbarColor.AsColors().thumb.CalcColor(aStyle);
  } else if (isActive || isHovered) {
    color = LookAndFeel::GetColor(LookAndFeel::ColorID::Highlight);
  } else {
    color = LookAndFeel::GetColor(LookAndFeel::ColorID::TextForeground);
  }

  return gfx::sRGBColor::FromABGR(color);
}

std::array<sRGBColor, 3> nsNativeBasicThemeWin::ComputeScrollbarButtonColors(
    nsIFrame* aFrame, StyleAppearance aAppearance, const ComputedStyle& aStyle,
    const EventStates& aElementState, const EventStates& aDocumentState) {
  if (!LookAndFeel::GetInt(LookAndFeel::IntID::UseAccessibilityTheme, 0)) {
    nscolor trackColor = ScrollbarUtil::GetScrollbarTrackColor(aFrame);
    nscolor buttonColor =
        ScrollbarUtil::GetScrollbarButtonColor(trackColor, aElementState);
    nscolor arrowColor = ScrollbarUtil::GetScrollbarArrowColor(buttonColor);
    return {sRGBColor::FromABGR(buttonColor), sRGBColor::FromABGR(arrowColor),
            sRGBColor::FromABGR(buttonColor)};
  }

  bool isActive = aElementState.HasState(NS_EVENT_STATE_ACTIVE);
  bool isHovered = aElementState.HasState(NS_EVENT_STATE_HOVER);

  sRGBColor buttonColor;
  sRGBColor arrowColor;
  if (isActive || isHovered) {
    buttonColor = sRGBColor::FromABGR(
        LookAndFeel::GetColor(LookAndFeel::ColorID::Highlight));
    arrowColor = sRGBColor::FromABGR(
        LookAndFeel::GetColor(LookAndFeel::ColorID::Buttonface));
  } else {
    buttonColor = sRGBColor::FromABGR(
        LookAndFeel::GetColor(LookAndFeel::ColorID::TextBackground));
    arrowColor = sRGBColor::FromABGR(
        LookAndFeel::GetColor(LookAndFeel::ColorID::TextForeground));
  }

  return {buttonColor, arrowColor, buttonColor};
}

already_AddRefed<nsITheme> do_GetBasicNativeThemeDoNotUseDirectly() {
  static StaticRefPtr<nsITheme> gInstance;
  if (MOZ_UNLIKELY(!gInstance)) {
    gInstance = new nsNativeBasicThemeWin();
    ClearOnShutdown(&gInstance);
  }
  return do_AddRef(gInstance);
}
