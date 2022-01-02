/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_WidgetMessageUtils_h
#define mozilla_WidgetMessageUtils_h

#include "ipc/EnumSerializer.h"
#include "ipc/IPCMessageUtils.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/widget/ThemeChangeKind.h"
#include "nsIWidget.h"
#include "nsStyleConsts.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::widget::ThemeChangeKind>
    : public BitFlagsEnumSerializer<mozilla::widget::ThemeChangeKind,
                                    mozilla::widget::ThemeChangeKind::AllBits> {
};

template <>
struct ParamTraits<mozilla::LookAndFeel::IntID>
    : ContiguousEnumSerializer<mozilla::LookAndFeel::IntID,
                               mozilla::LookAndFeel::IntID::CaretBlinkTime,
                               mozilla::LookAndFeel::IntID::End> {
  using IdType = std::underlying_type_t<mozilla::LookAndFeel::IntID>;
  static_assert(static_cast<IdType>(
                    mozilla::LookAndFeel::IntID::CaretBlinkTime) == IdType(0));
};

template <>
struct ParamTraits<mozilla::LookAndFeel::ColorID>
    : ContiguousEnumSerializer<
          mozilla::LookAndFeel::ColorID,
          mozilla::LookAndFeel::ColorID::TextSelectDisabledBackground,
          mozilla::LookAndFeel::ColorID::End> {
  using IdType = std::underlying_type_t<mozilla::LookAndFeel::ColorID>;
  static_assert(
      static_cast<IdType>(
          mozilla::LookAndFeel::ColorID::TextSelectDisabledBackground) ==
      IdType(0));
};

template <>
struct ParamTraits<nsTransparencyMode>
    : ContiguousEnumSerializerInclusive<nsTransparencyMode, eTransparencyOpaque,
                                        eTransparencyBorderlessGlass> {};

template <>
struct ParamTraits<nsCursor>
    : ContiguousEnumSerializer<nsCursor, eCursor_standard, eCursorCount> {};

template <>
struct ParamTraits<nsIWidget::TouchpadGesturePhase>
    : ContiguousEnumSerializerInclusive<
          nsIWidget::TouchpadGesturePhase,
          nsIWidget::TouchpadGesturePhase::PHASE_BEGIN,
          nsIWidget::TouchpadGesturePhase::PHASE_END> {};

template <>
struct ParamTraits<nsIWidget::TouchPointerState>
    : public BitFlagsEnumSerializer<nsIWidget::TouchPointerState,
                                    nsIWidget::TouchPointerState::ALL_BITS> {};

}  // namespace IPC

#endif  // WidgetMessageUtils_h
