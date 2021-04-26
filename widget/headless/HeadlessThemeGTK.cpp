/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HeadlessThemeGTK.h"

#include "mozilla/StaticPrefs_layout.h"
#include "nsStyleConsts.h"
#include "nsIFrame.h"

namespace mozilla {
namespace widget {

NS_IMPL_ISUPPORTS_INHERITED(HeadlessThemeGTK, nsNativeTheme, nsITheme)

NS_IMETHODIMP
HeadlessThemeGTK::DrawWidgetBackground(gfxContext* aContext, nsIFrame* aFrame,
                                       StyleAppearance aAppearance,
                                       const nsRect& aRect,
                                       const nsRect& aDirtyRect, DrawOverflow) {
  return NS_OK;
}

LayoutDeviceIntMargin HeadlessThemeGTK::GetWidgetBorder(
    nsDeviceContext* aContext, nsIFrame* aFrame, StyleAppearance aAppearance) {
  LayoutDeviceIntMargin result;
  // The following values are generated from the Ubuntu GTK theme.
  switch (aAppearance) {
    case StyleAppearance::Button:
    case StyleAppearance::Toolbarbutton:
      result.top = 6;
      result.right = 7;
      result.bottom = 6;
      result.left = 7;
      break;
    case StyleAppearance::FocusOutline:
    case StyleAppearance::NumberInput:
    case StyleAppearance::Textfield:
      result.top = 5;
      result.right = 7;
      result.bottom = 5;
      result.left = 7;
      break;
    case StyleAppearance::Statusbarpanel:
    case StyleAppearance::Resizerpanel:
    case StyleAppearance::Listbox:
    case StyleAppearance::Treeview:
    case StyleAppearance::Treeheadersortarrow:
    case StyleAppearance::ProgressBar:
    case StyleAppearance::SpinnerUpbutton:
    case StyleAppearance::SpinnerDownbutton:
    case StyleAppearance::SpinnerTextfield:
    case StyleAppearance::Textarea:
    case StyleAppearance::Menupopup:
    case StyleAppearance::MozGtkInfoBar:
      result.top = 1;
      result.right = 1;
      result.bottom = 1;
      result.left = 1;
      break;
    case StyleAppearance::Treeheadercell:
      result.top = 5;
      result.right = 7;
      result.bottom = 6;
      result.left = 6;
      break;
    case StyleAppearance::Tab:
      result.top = 4;
      result.right = 7;
      result.bottom = 2;
      result.left = 7;
      break;
    case StyleAppearance::Tooltip:
      result.top = 6;
      result.right = 6;
      result.bottom = 6;
      result.left = 6;
      break;
    case StyleAppearance::MenulistButton:
    case StyleAppearance::Menulist:
      result.top = 6;
      result.right = 22;
      result.bottom = 6;
      result.left = 7;
      break;
    case StyleAppearance::MozMenulistArrowButton:
      result.top = 1;
      result.right = 1;
      result.bottom = 1;
      result.left = 0;
      break;
    case StyleAppearance::Menuitem:
    case StyleAppearance::Checkmenuitem:
    case StyleAppearance::Radiomenuitem:
      if (IsRegularMenuItem(aFrame)) {
        break;
      }
      result.top = 3;
      result.right = 5;
      result.bottom = 3;
      result.left = 5;
      break;
    default:
      break;
  }
  return result;
}

bool HeadlessThemeGTK::GetWidgetPadding(nsDeviceContext* aContext,
                                        nsIFrame* aFrame,
                                        StyleAppearance aAppearance,
                                        LayoutDeviceIntMargin* aResult) {
  // The following values are generated from the Ubuntu GTK theme.
  switch (aAppearance) {
    case StyleAppearance::Radio:
    case StyleAppearance::Checkbox:
    case StyleAppearance::Toolbarbutton:
    case StyleAppearance::Dualbutton:
    case StyleAppearance::ToolbarbuttonDropdown:
    case StyleAppearance::ButtonArrowUp:
    case StyleAppearance::ButtonArrowDown:
    case StyleAppearance::ButtonArrowNext:
    case StyleAppearance::ButtonArrowPrevious:
    case StyleAppearance::TabScrollArrowBack:
    case StyleAppearance::TabScrollArrowForward:
    case StyleAppearance::MozMenulistArrowButton:
    case StyleAppearance::RangeThumb:
    case StyleAppearance::ButtonFocus:
      aResult->top = 0;
      aResult->right = 0;
      aResult->bottom = 0;
      aResult->left = 0;
      return true;
    case StyleAppearance::Menuitem:
    case StyleAppearance::Checkmenuitem:
    case StyleAppearance::Radiomenuitem:
      if (!IsRegularMenuItem(aFrame)) {
        return false;
      }
      aResult->top = 3;
      aResult->right = 5;
      aResult->bottom = 3;
      aResult->left = 5;
      return true;
    default:
      break;
  }
  return false;
}

static const int32_t kMinimumScrollbarSize = 10;

// TODO: Should probably deal with scrollbar-width somehow.
auto HeadlessThemeGTK::GetScrollbarSizes(nsPresContext*, StyleScrollbarWidth,
                                         Overlay) -> ScrollbarSizes {
  return {kMinimumScrollbarSize, kMinimumScrollbarSize};
}

NS_IMETHODIMP
HeadlessThemeGTK::GetMinimumWidgetSize(nsPresContext* aPresContext,
                                       nsIFrame* aFrame,
                                       StyleAppearance aAppearance,
                                       LayoutDeviceIntSize* aResult,
                                       bool* aIsOverridable) {
  aResult->width = aResult->height = 0;
  *aIsOverridable = true;

  // The following values are generated from the Ubuntu GTK theme.
  switch (aAppearance) {
    case StyleAppearance::Splitter:
      if (IsHorizontal(aFrame)) {
        aResult->width = 6;
        aResult->height = 0;
      } else {
        aResult->width = 0;
        aResult->height = 6;
      }
      *aIsOverridable = false;
      break;
    case StyleAppearance::Button:
    case StyleAppearance::Toolbarbutton:
      aResult->width = 14;
      aResult->height = 12;
      break;
    case StyleAppearance::Radio:
    case StyleAppearance::Checkbox:
      aResult->width = 18;
      aResult->height = 18;
      break;
    case StyleAppearance::ToolbarbuttonDropdown:
    case StyleAppearance::ButtonArrowUp:
    case StyleAppearance::ButtonArrowDown:
    case StyleAppearance::ButtonArrowNext:
    case StyleAppearance::ButtonArrowPrevious:
    case StyleAppearance::Resizer:
      aResult->width = 15;
      aResult->height = 15;
      *aIsOverridable = false;
      break;
    case StyleAppearance::Separator:
      aResult->width = 12;
      aResult->height = 0;
      break;
    case StyleAppearance::Treetwisty:
    case StyleAppearance::Treetwistyopen:
      aResult->width = 8;
      aResult->height = 8;
      *aIsOverridable = false;
      break;
    case StyleAppearance::Treeheadercell:
      aResult->width = 13;
      aResult->height = 11;
      break;
    case StyleAppearance::Treeheadersortarrow:
    case StyleAppearance::SpinnerUpbutton:
    case StyleAppearance::SpinnerDownbutton:
      aResult->width = 14;
      aResult->height = 13;
      break;
    case StyleAppearance::TabScrollArrowBack:
    case StyleAppearance::TabScrollArrowForward:
      aResult->width = 16;
      aResult->height = 16;
      *aIsOverridable = false;
      break;
    case StyleAppearance::Spinner:
      aResult->width = 14;
      aResult->height = 26;
      break;
    case StyleAppearance::NumberInput:
    case StyleAppearance::Textfield:
      aResult->width = 0;
      aResult->height = 12;
      break;
    case StyleAppearance::ScrollbarHorizontal:
      aResult->width = 31;
      aResult->height = kMinimumScrollbarSize;
      break;
    case StyleAppearance::ScrollbarVertical:
      aResult->width = kMinimumScrollbarSize;
      aResult->height = 31;
      break;
    case StyleAppearance::ScrollbarbuttonUp:
    case StyleAppearance::ScrollbarbuttonDown:
      aResult->width = 10;
      aResult->height = 13;
      *aIsOverridable = false;
      break;
    case StyleAppearance::ScrollbarbuttonLeft:
    case StyleAppearance::ScrollbarbuttonRight:
      aResult->width = 13;
      aResult->height = 10;
      *aIsOverridable = false;
      break;
    case StyleAppearance::ScrollbarthumbHorizontal:
      aResult->width = 31;
      aResult->height = 10;
      *aIsOverridable = false;
      break;
    case StyleAppearance::ScrollbarthumbVertical:
      aResult->width = 10;
      aResult->height = 31;
      *aIsOverridable = false;
      break;
    case StyleAppearance::MenulistButton:
    case StyleAppearance::Menulist:
      aResult->width = 44;
      aResult->height = 27;
      break;
    case StyleAppearance::MozMenulistArrowButton:
      aResult->width = 29;
      aResult->height = 28;
      *aIsOverridable = false;
      break;
    case StyleAppearance::RangeThumb:
      aResult->width = 14;
      aResult->height = 18;
      *aIsOverridable = false;
      break;
    case StyleAppearance::Menuseparator:
      aResult->width = 0;
      aResult->height = 8;
      *aIsOverridable = false;
      break;
    default:
      break;
  }
  return NS_OK;
}

NS_IMETHODIMP
HeadlessThemeGTK::WidgetStateChanged(nsIFrame* aFrame,
                                     StyleAppearance aAppearance,
                                     nsAtom* aAttribute, bool* aShouldRepaint,
                                     const nsAttrValue* aOldValue) {
  return NS_OK;
}

NS_IMETHODIMP
HeadlessThemeGTK::ThemeChanged() { return NS_OK; }

static bool IsFrameContentNodeInNamespace(nsIFrame* aFrame,
                                          uint32_t aNamespace) {
  nsIContent* content = aFrame ? aFrame->GetContent() : nullptr;
  if (!content) return false;
  return content->IsInNamespace(aNamespace);
}

NS_IMETHODIMP_(bool)
HeadlessThemeGTK::ThemeSupportsWidget(nsPresContext* aPresContext,
                                      nsIFrame* aFrame,
                                      StyleAppearance aAppearance) {
  switch (aAppearance) {
    case StyleAppearance::Button:
    case StyleAppearance::Radio:
    case StyleAppearance::Checkbox:
    case StyleAppearance::FocusOutline:
    case StyleAppearance::Toolbox:
    case StyleAppearance::Toolbar:
    case StyleAppearance::Toolbarbutton:
    case StyleAppearance::Dualbutton:
    case StyleAppearance::ToolbarbuttonDropdown:
    case StyleAppearance::ButtonArrowUp:
    case StyleAppearance::ButtonArrowDown:
    case StyleAppearance::ButtonArrowNext:
    case StyleAppearance::ButtonArrowPrevious:
    case StyleAppearance::Separator:
    case StyleAppearance::Toolbargripper:
    case StyleAppearance::Splitter:
    case StyleAppearance::Statusbar:
    case StyleAppearance::Statusbarpanel:
    case StyleAppearance::Resizerpanel:
    case StyleAppearance::Resizer:
    case StyleAppearance::Listbox:
    case StyleAppearance::Treeview:
    case StyleAppearance::Treetwisty:
    case StyleAppearance::Treeheadercell:
    case StyleAppearance::Treeheadersortarrow:
    case StyleAppearance::Treetwistyopen:
    case StyleAppearance::ProgressBar:
    case StyleAppearance::Progresschunk:
    case StyleAppearance::Tab:
    case StyleAppearance::Tabpanels:
    case StyleAppearance::TabScrollArrowBack:
    case StyleAppearance::TabScrollArrowForward:
    case StyleAppearance::Tooltip:
    case StyleAppearance::Spinner:
    case StyleAppearance::SpinnerUpbutton:
    case StyleAppearance::SpinnerDownbutton:
    case StyleAppearance::SpinnerTextfield:
    case StyleAppearance::NumberInput:
    case StyleAppearance::ScrollbarHorizontal:
    case StyleAppearance::ScrollbarVertical:
    case StyleAppearance::ScrollbarbuttonUp:
    case StyleAppearance::ScrollbarbuttonDown:
    case StyleAppearance::ScrollbarbuttonLeft:
    case StyleAppearance::ScrollbarbuttonRight:
    case StyleAppearance::ScrollbartrackHorizontal:
    case StyleAppearance::ScrollbartrackVertical:
    case StyleAppearance::ScrollbarthumbHorizontal:
    case StyleAppearance::ScrollbarthumbVertical:
    case StyleAppearance::Textfield:
    case StyleAppearance::Textarea:
    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton:
    case StyleAppearance::MenulistText:
    case StyleAppearance::Range:
    case StyleAppearance::RangeThumb:
    case StyleAppearance::CheckboxContainer:
    case StyleAppearance::RadioContainer:
    case StyleAppearance::CheckboxLabel:
    case StyleAppearance::RadioLabel:
    case StyleAppearance::ButtonFocus:
    case StyleAppearance::Window:
    case StyleAppearance::Dialog:
    case StyleAppearance::Menubar:
    case StyleAppearance::Menupopup:
    case StyleAppearance::Menuitem:
    case StyleAppearance::Checkmenuitem:
    case StyleAppearance::Radiomenuitem:
    case StyleAppearance::Menuseparator:
    case StyleAppearance::Menuarrow:
    case StyleAppearance::MozGtkInfoBar:
      return !IsWidgetStyled(aPresContext, aFrame, aAppearance);
    case StyleAppearance::MozMenulistArrowButton:
      return (!aFrame ||
              IsFrameContentNodeInNamespace(aFrame, kNameSpaceID_XUL)) &&
             !IsWidgetStyled(aPresContext, aFrame, aAppearance);
    default:
      break;
  }
  return false;
}

NS_IMETHODIMP_(bool)
HeadlessThemeGTK::WidgetIsContainer(StyleAppearance aAppearance) {
  if (aAppearance == StyleAppearance::MozMenulistArrowButton ||
      aAppearance == StyleAppearance::Radio ||
      aAppearance == StyleAppearance::RangeThumb ||
      aAppearance == StyleAppearance::Checkbox ||
      aAppearance == StyleAppearance::TabScrollArrowBack ||
      aAppearance == StyleAppearance::TabScrollArrowForward ||
      aAppearance == StyleAppearance::ButtonArrowUp ||
      aAppearance == StyleAppearance::ButtonArrowDown ||
      aAppearance == StyleAppearance::ButtonArrowNext ||
      aAppearance == StyleAppearance::ButtonArrowPrevious) {
    return false;
  }
  return true;
}

bool HeadlessThemeGTK::ThemeDrawsFocusForWidget(StyleAppearance aAppearance) {
  if (aAppearance == StyleAppearance::Menulist ||
      aAppearance == StyleAppearance::Button ||
      aAppearance == StyleAppearance::Treeheadercell) {
    return true;
  }
  return false;
}

bool HeadlessThemeGTK::ThemeNeedsComboboxDropmarker() { return false; }

}  // namespace widget
}  // namespace mozilla
