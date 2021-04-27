/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This defines a common base class for nsITheme implementations, to reduce
// code duplication.

#ifndef _NSNATIVETHEME_H_
#define _NSNATIVETHEME_H_

#include "nsAlgorithm.h"
#include "nsAtom.h"
#include "nsColor.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsMargin.h"
#include "nsGkAtoms.h"
#include "nsTArray.h"
#include "nsINamed.h"
#include "nsITimer.h"
#include "nsIContent.h"

class nsIFrame;
class nsPresContext;

namespace mozilla {
class ComputedStyle;
enum class StyleAppearance : uint8_t;
class EventStates;
}  // namespace mozilla

class nsNativeTheme : public nsITimerCallback, public nsINamed {
 protected:
  virtual ~nsNativeTheme() = default;

  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED

  nsNativeTheme();

 public:
  enum ScrollbarButtonType {
    eScrollbarButton_UpTop = 0,
    eScrollbarButton_Down = 1 << 0,
    eScrollbarButton_Bottom = 1 << 1
  };

  enum TreeSortDirection {
    eTreeSortDirection_Descending,
    eTreeSortDirection_Natural,
    eTreeSortDirection_Ascending
  };
  // Returns the content state (hover, focus, etc), see EventStateManager.h
  static mozilla::EventStates GetContentState(
      nsIFrame* aFrame, mozilla::StyleAppearance aAppearance);

  // Returns whether the widget is already styled by content
  // Normally called from ThemeSupportsWidget to turn off native theming
  // for elements that are already styled.
  bool IsWidgetStyled(nsPresContext* aPresContext, nsIFrame* aFrame,
                      mozilla::StyleAppearance aAppearance);

  // Accessors to widget-specific state information

  bool IsDisabled(nsIFrame* aFrame, mozilla::EventStates aEventStates);

  // RTL chrome direction
  static bool IsFrameRTL(nsIFrame* aFrame);

  static bool IsHTMLContent(nsIFrame* aFrame);

  // button:
  bool IsDefaultButton(nsIFrame* aFrame) {
    return CheckBooleanAttr(aFrame, nsGkAtoms::_default);
  }

  bool IsButtonTypeMenu(nsIFrame* aFrame);

  // checkbox:
  bool IsChecked(nsIFrame* aFrame) {
    return GetCheckedOrSelected(aFrame, false);
  }

  // radiobutton:
  bool IsSelected(nsIFrame* aFrame) {
    return GetCheckedOrSelected(aFrame, true);
  }

  static bool IsFocused(nsIFrame* aFrame) {
    return CheckBooleanAttr(aFrame, nsGkAtoms::focused);
  }

  // scrollbar button:
  int32_t GetScrollbarButtonType(nsIFrame* aFrame);

  // tab:
  bool IsSelectedTab(nsIFrame* aFrame) {
    return CheckBooleanAttr(aFrame, nsGkAtoms::visuallyselected);
  }

  bool IsNextToSelectedTab(nsIFrame* aFrame, int32_t aOffset);

  bool IsBeforeSelectedTab(nsIFrame* aFrame) {
    return IsNextToSelectedTab(aFrame, -1);
  }

  bool IsAfterSelectedTab(nsIFrame* aFrame) {
    return IsNextToSelectedTab(aFrame, 1);
  }

  bool IsLeftToSelectedTab(nsIFrame* aFrame) {
    return IsFrameRTL(aFrame) ? IsAfterSelectedTab(aFrame)
                              : IsBeforeSelectedTab(aFrame);
  }

  bool IsRightToSelectedTab(nsIFrame* aFrame) {
    return IsFrameRTL(aFrame) ? IsBeforeSelectedTab(aFrame)
                              : IsAfterSelectedTab(aFrame);
  }

  // button / toolbarbutton:
  bool IsCheckedButton(nsIFrame* aFrame) {
    return CheckBooleanAttr(aFrame, nsGkAtoms::checked);
  }

  bool IsSelectedButton(nsIFrame* aFrame) {
    return CheckBooleanAttr(aFrame, nsGkAtoms::checked) ||
           CheckBooleanAttr(aFrame, nsGkAtoms::selected);
  }

  bool IsOpenButton(nsIFrame* aFrame) {
    return CheckBooleanAttr(aFrame, nsGkAtoms::open);
  }

  bool IsPressedButton(nsIFrame* aFrame);

  // treeheadercell:
  TreeSortDirection GetTreeSortDirection(nsIFrame* aFrame);
  bool IsLastTreeHeaderCell(nsIFrame* aFrame);

  // tab:
  bool IsBottomTab(nsIFrame* aFrame);
  bool IsFirstTab(nsIFrame* aFrame);

  bool IsHorizontal(nsIFrame* aFrame);

  // progressbar:
  bool IsIndeterminateProgress(nsIFrame* aFrame,
                               mozilla::EventStates aEventStates);
  bool IsVerticalProgress(nsIFrame* aFrame);

  // meter:
  bool IsVerticalMeter(nsIFrame* aFrame);

  // textfield:
  bool IsReadOnly(nsIFrame* aFrame) {
    return CheckBooleanAttr(aFrame, nsGkAtoms::readonly);
  }

  // menupopup:
  bool IsSubmenu(nsIFrame* aFrame, bool* aLeftOfParent);

  // True if it's not a menubar item or menulist item
  bool IsRegularMenuItem(nsIFrame* aFrame);

  static bool CheckBooleanAttr(nsIFrame* aFrame, nsAtom* aAtom);
  static int32_t CheckIntAttr(nsIFrame* aFrame, nsAtom* aAtom,
                              int32_t defaultValue);

  // Helpers for progressbar.
  static double GetProgressValue(nsIFrame* aFrame);
  static double GetProgressMaxValue(nsIFrame* aFrame);

  bool GetCheckedOrSelected(nsIFrame* aFrame, bool aCheckSelected);
  bool GetIndeterminate(nsIFrame* aFrame);

  bool QueueAnimatedContentForRefresh(nsIContent* aContent,
                                      uint32_t aMinimumFrameRate);

  nsIFrame* GetAdjacentSiblingFrameWithSameAppearance(nsIFrame* aFrame,
                                                      bool aNextSibling);

  bool IsRangeHorizontal(nsIFrame* aFrame);

  static bool IsDarkBackground(nsIFrame* aFrame);
  static bool IsDarkColor(nscolor aColor) {
    // Consider a color dark if the sum of the r, g and b values is less than
    // 384 in a semi-transparent document.  This heuristic matches what WebKit
    // does, and we can improve it later if needed.
    return NS_GET_A(aColor) > 127 &&
           NS_GET_R(aColor) + NS_GET_G(aColor) + NS_GET_B(aColor) < 384;
  }

  static bool IsWidgetScrollbarPart(mozilla::StyleAppearance aAppearance);

 private:
  uint32_t mAnimatedContentTimeout;
  nsCOMPtr<nsITimer> mAnimatedContentTimer;
  AutoTArray<nsCOMPtr<nsIContent>, 20> mAnimatedContentList;
};

#endif  // _NSNATIVETHEME_H_
