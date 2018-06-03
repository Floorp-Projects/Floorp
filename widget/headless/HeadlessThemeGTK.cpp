/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HeadlessThemeGTK.h"
#include "nsThemeConstants.h"
#include "nsIFrame.h"


namespace mozilla {
namespace widget {

NS_IMPL_ISUPPORTS_INHERITED(HeadlessThemeGTK, nsNativeTheme, nsITheme)

NS_IMETHODIMP
HeadlessThemeGTK::DrawWidgetBackground(gfxContext* aContext,
                                       nsIFrame* aFrame,
                                       uint8_t aWidgetType,
                                       const nsRect& aRect,
                                       const nsRect& aDirtyRect)
{
  return NS_OK;
}

LayoutDeviceIntMargin
HeadlessThemeGTK::GetWidgetBorder(nsDeviceContext* aContext, nsIFrame* aFrame,
                                  uint8_t aWidgetType)
{
  LayoutDeviceIntMargin result;
  // The following values are generated from the Ubuntu GTK theme.
  switch (aWidgetType) {
    case NS_THEME_BUTTON:
    case NS_THEME_TOOLBARBUTTON:
      result.top = 6;
      result.right = 7;
      result.bottom = 6;
      result.left = 7;
      break;
    case NS_THEME_FOCUS_OUTLINE:
    case NS_THEME_NUMBER_INPUT:
    case NS_THEME_TEXTFIELD:
      result.top = 5;
      result.right = 7;
      result.bottom = 5;
      result.left = 7;
      break;
    case NS_THEME_STATUSBARPANEL:
    case NS_THEME_RESIZERPANEL:
    case NS_THEME_LISTBOX:
    case NS_THEME_TREEVIEW:
    case NS_THEME_TREEHEADERSORTARROW:
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_SPINNER_UPBUTTON:
    case NS_THEME_SPINNER_DOWNBUTTON:
    case NS_THEME_SPINNER_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:
    case NS_THEME_MENUPOPUP:
    case NS_THEME_GTK_INFO_BAR:
      result.top = 1;
      result.right = 1;
      result.bottom = 1;
      result.left = 1;
      break;
    case NS_THEME_TREEHEADERCELL:
      result.top = 5;
      result.right = 7;
      result.bottom = 6;
      result.left = 6;
      break;
    case NS_THEME_TAB:
      result.top = 4;
      result.right = 7;
      result.bottom = 2;
      result.left = 7;
      break;
    case NS_THEME_TOOLTIP:
      result.top = 6;
      result.right = 6;
      result.bottom = 6;
      result.left = 6;
      break;
    case NS_THEME_MENULIST:
      result.top = 6;
      result.right = 22;
      result.bottom = 6;
      result.left = 7;
      break;
    case NS_THEME_MENULIST_BUTTON:
      result.top = 1;
      result.right = 1;
      result.bottom = 1;
      result.left = 0;
      break;
    case NS_THEME_MENULIST_TEXTFIELD:
      result.top = 1;
      result.right = 0;
      result.bottom = 1;
      result.left = 1;
      break;
    case NS_THEME_MENUITEM:
    case NS_THEME_CHECKMENUITEM:
    case NS_THEME_RADIOMENUITEM:
      if (IsRegularMenuItem(aFrame)) {
        break;
      }
      result.top = 3;
      result.right = 5;
      result.bottom = 3;
      result.left = 5;
      break;
  }
  return result;
}

bool
HeadlessThemeGTK::GetWidgetPadding(nsDeviceContext* aContext,
                                   nsIFrame* aFrame, uint8_t aWidgetType,
                                   LayoutDeviceIntMargin* aResult)
{
  // The following values are generated from the Ubuntu GTK theme.
  switch (aWidgetType) {
    case NS_THEME_RADIO:
    case NS_THEME_CHECKBOX:
    case NS_THEME_TOOLBARBUTTON:
    case NS_THEME_DUALBUTTON:
    case NS_THEME_TOOLBARBUTTON_DROPDOWN:
    case NS_THEME_BUTTON_ARROW_UP:
    case NS_THEME_BUTTON_ARROW_DOWN:
    case NS_THEME_BUTTON_ARROW_NEXT:
    case NS_THEME_BUTTON_ARROW_PREVIOUS:
    case NS_THEME_TAB_SCROLL_ARROW_BACK:
    case NS_THEME_TAB_SCROLL_ARROW_FORWARD:
    case NS_THEME_MENULIST_BUTTON:
    case NS_THEME_RANGE_THUMB:
    case NS_THEME_BUTTON_FOCUS:
      aResult->top = 0;
      aResult->right = 0;
      aResult->bottom = 0;
      aResult->left = 0;
      return true;
    case NS_THEME_MENUITEM:
    case NS_THEME_CHECKMENUITEM:
    case NS_THEME_RADIOMENUITEM:
      if (!IsRegularMenuItem(aFrame)) {
        return false;
      }
      aResult->top = 3;
      aResult->right = 5;
      aResult->bottom = 3;
      aResult->left = 5;
      return true;
  }
  return false;
}


NS_IMETHODIMP
HeadlessThemeGTK::GetMinimumWidgetSize(nsPresContext* aPresContext,
                                       nsIFrame* aFrame, uint8_t aWidgetType,
                                       LayoutDeviceIntSize* aResult,
                                       bool* aIsOverridable)
{
  aResult->width = aResult->height = 0;
  *aIsOverridable = true;

  // The following values are generated from the Ubuntu GTK theme.
  switch (aWidgetType) {
    case NS_THEME_SPLITTER:
      if (IsHorizontal(aFrame)) {
        aResult->width = 6;
        aResult->height = 0;
      } else {
        aResult->width = 0;
        aResult->height = 6;
      }
      *aIsOverridable = false;
      break;
    case NS_THEME_BUTTON:
    case NS_THEME_TOOLBARBUTTON:
      aResult->width = 14;
      aResult->height = 12;
      break;
    case NS_THEME_RADIO:
    case NS_THEME_CHECKBOX:
      aResult->width = 18;
      aResult->height = 18;
      break;
    case NS_THEME_TOOLBARBUTTON_DROPDOWN:
    case NS_THEME_BUTTON_ARROW_UP:
    case NS_THEME_BUTTON_ARROW_DOWN:
    case NS_THEME_BUTTON_ARROW_NEXT:
    case NS_THEME_BUTTON_ARROW_PREVIOUS:
    case NS_THEME_RESIZER:
      aResult->width = 15;
      aResult->height = 15;
      *aIsOverridable = false;
      break;
    case NS_THEME_SEPARATOR:
      aResult->width = 12;
      aResult->height = 0;
      break;
    case NS_THEME_TREETWISTY:
    case NS_THEME_TREETWISTYOPEN:
      aResult->width = 8;
      aResult->height = 8;
      *aIsOverridable = false;
      break;
    case NS_THEME_TREEHEADERCELL:
      aResult->width = 13;
      aResult->height = 11;
      break;
    case NS_THEME_TREEHEADERSORTARROW:
    case NS_THEME_SPINNER_UPBUTTON:
    case NS_THEME_SPINNER_DOWNBUTTON:
      aResult->width = 14;
      aResult->height = 13;
      break;
    case NS_THEME_TAB_SCROLL_ARROW_BACK:
    case NS_THEME_TAB_SCROLL_ARROW_FORWARD:
      aResult->width = 16;
      aResult->height = 16;
      *aIsOverridable = false;
      break;
    case NS_THEME_INNER_SPIN_BUTTON:
    case NS_THEME_SPINNER:
      aResult->width = 14;
      aResult->height = 26;
      break;
    case NS_THEME_NUMBER_INPUT:
    case NS_THEME_TEXTFIELD:
      aResult->width = 0;
      aResult->height = 12;
      break;
    case NS_THEME_SCROLLBAR_HORIZONTAL:
      aResult->width = 31;
      aResult->height = 10;
      break;
    case NS_THEME_SCROLLBAR_VERTICAL:
      aResult->width = 10;
      aResult->height = 31;
      break;
    case NS_THEME_SCROLLBARBUTTON_UP:
    case NS_THEME_SCROLLBARBUTTON_DOWN:
      aResult->width = 10;
      aResult->height = 13;
      *aIsOverridable = false;
      break;
    case NS_THEME_SCROLLBARBUTTON_LEFT:
    case NS_THEME_SCROLLBARBUTTON_RIGHT:
      aResult->width = 13;
      aResult->height = 10;
      *aIsOverridable = false;
      break;
    case NS_THEME_SCROLLBARTHUMB_HORIZONTAL:
      aResult->width = 31;
      aResult->height = 10;
      *aIsOverridable = false;
      break;
    case NS_THEME_SCROLLBARTHUMB_VERTICAL:
      aResult->width = 10;
      aResult->height = 31;
      *aIsOverridable = false;
      break;
    case NS_THEME_MENULIST:
      aResult->width = 44;
      aResult->height = 27;
      break;
    case NS_THEME_MENULIST_BUTTON:
      aResult->width = 29;
      aResult->height = 28;
      *aIsOverridable = false;
      break;
    case NS_THEME_SCALETHUMB_HORIZONTAL:
    case NS_THEME_RANGE_THUMB:
      aResult->width = 14;
      aResult->height = 18;
      *aIsOverridable = false;
      break;
    case NS_THEME_SCALETHUMB_VERTICAL:
      aResult->width = 18;
      aResult->height = 13;
      *aIsOverridable = false;
      break;
    case NS_THEME_RANGE:
      aResult->width = 14;
      aResult->height = 18;
      break;
    case NS_THEME_MENUSEPARATOR:
      aResult->width = 0;
      aResult->height = 8;
      *aIsOverridable = false;
      break;
  }
  return NS_OK;
}

NS_IMETHODIMP
HeadlessThemeGTK::WidgetStateChanged(nsIFrame* aFrame, uint8_t aWidgetType,
                                     nsAtom* aAttribute, bool* aShouldRepaint,
                                     const nsAttrValue* aOldValue)
{
  return NS_OK;
}

NS_IMETHODIMP
HeadlessThemeGTK::ThemeChanged()
{
  return NS_OK;
}

static bool IsFrameContentNodeInNamespace(nsIFrame *aFrame, uint32_t aNamespace)
{
  nsIContent *content = aFrame ? aFrame->GetContent() : nullptr;
  if (!content)
    return false;
  return content->IsInNamespace(aNamespace);
}

NS_IMETHODIMP_(bool)
HeadlessThemeGTK::ThemeSupportsWidget(nsPresContext* aPresContext,
                                      nsIFrame* aFrame,
                                      uint8_t aWidgetType)
{
  switch (aWidgetType) {
    case NS_THEME_BUTTON:
    case NS_THEME_RADIO:
    case NS_THEME_CHECKBOX:
    case NS_THEME_FOCUS_OUTLINE:
    case NS_THEME_TOOLBOX:
    case NS_THEME_TOOLBAR:
    case NS_THEME_TOOLBARBUTTON:
    case NS_THEME_DUALBUTTON:
    case NS_THEME_TOOLBARBUTTON_DROPDOWN:
    case NS_THEME_BUTTON_ARROW_UP:
    case NS_THEME_BUTTON_ARROW_DOWN:
    case NS_THEME_BUTTON_ARROW_NEXT:
    case NS_THEME_BUTTON_ARROW_PREVIOUS:
    case NS_THEME_SEPARATOR:
    case NS_THEME_TOOLBARGRIPPER:
    case NS_THEME_SPLITTER:
    case NS_THEME_STATUSBAR:
    case NS_THEME_STATUSBARPANEL:
    case NS_THEME_RESIZERPANEL:
    case NS_THEME_RESIZER:
    case NS_THEME_LISTBOX:
    case NS_THEME_TREEVIEW:
    case NS_THEME_TREETWISTY:
    case NS_THEME_TREEHEADERCELL:
    case NS_THEME_TREEHEADERSORTARROW:
    case NS_THEME_TREETWISTYOPEN:
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSCHUNK:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_PROGRESSCHUNK_VERTICAL:
    case NS_THEME_TAB:
    case NS_THEME_TABPANELS:
    case NS_THEME_TAB_SCROLL_ARROW_BACK:
    case NS_THEME_TAB_SCROLL_ARROW_FORWARD:
    case NS_THEME_TOOLTIP:
    case NS_THEME_INNER_SPIN_BUTTON:
    case NS_THEME_SPINNER:
    case NS_THEME_SPINNER_UPBUTTON:
    case NS_THEME_SPINNER_DOWNBUTTON:
    case NS_THEME_SPINNER_TEXTFIELD:
    case NS_THEME_NUMBER_INPUT:
    case NS_THEME_SCROLLBAR_HORIZONTAL:
    case NS_THEME_SCROLLBAR_VERTICAL:
    case NS_THEME_SCROLLBARBUTTON_UP:
    case NS_THEME_SCROLLBARBUTTON_DOWN:
    case NS_THEME_SCROLLBARBUTTON_LEFT:
    case NS_THEME_SCROLLBARBUTTON_RIGHT:
    case NS_THEME_SCROLLBARTRACK_HORIZONTAL:
    case NS_THEME_SCROLLBARTRACK_VERTICAL:
    case NS_THEME_SCROLLBARTHUMB_HORIZONTAL:
    case NS_THEME_SCROLLBARTHUMB_VERTICAL:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:
    case NS_THEME_MENULIST:
    case NS_THEME_MENULIST_TEXT:
    case NS_THEME_MENULIST_TEXTFIELD:
    case NS_THEME_SCALE_HORIZONTAL:
    case NS_THEME_SCALE_VERTICAL:
    case NS_THEME_SCALETHUMB_HORIZONTAL:
    case NS_THEME_SCALETHUMB_VERTICAL:
    case NS_THEME_RANGE:
    case NS_THEME_RANGE_THUMB:
    case NS_THEME_CHECKBOX_CONTAINER:
    case NS_THEME_RADIO_CONTAINER:
    case NS_THEME_CHECKBOX_LABEL:
    case NS_THEME_RADIO_LABEL:
    case NS_THEME_BUTTON_FOCUS:
    case NS_THEME_WINDOW:
    case NS_THEME_DIALOG:
    case NS_THEME_MENUBAR:
    case NS_THEME_MENUPOPUP:
    case NS_THEME_MENUITEM:
    case NS_THEME_CHECKMENUITEM:
    case NS_THEME_RADIOMENUITEM:
    case NS_THEME_MENUSEPARATOR:
    case NS_THEME_MENUARROW:
    case NS_THEME_GTK_INFO_BAR:
      return !IsWidgetStyled(aPresContext, aFrame, aWidgetType);
    case NS_THEME_MENULIST_BUTTON:
      return (!aFrame || IsFrameContentNodeInNamespace(aFrame, kNameSpaceID_XUL)) &&
              !IsWidgetStyled(aPresContext, aFrame, aWidgetType);
  }
  return false;
}

NS_IMETHODIMP_(bool)
HeadlessThemeGTK::WidgetIsContainer(uint8_t aWidgetType)
{
    if (aWidgetType == NS_THEME_MENULIST_BUTTON ||
        aWidgetType == NS_THEME_RADIO ||
        aWidgetType == NS_THEME_RANGE_THUMB ||
        aWidgetType == NS_THEME_CHECKBOX ||
        aWidgetType == NS_THEME_TAB_SCROLL_ARROW_BACK ||
        aWidgetType == NS_THEME_TAB_SCROLL_ARROW_FORWARD ||
        aWidgetType == NS_THEME_BUTTON_ARROW_UP ||
        aWidgetType == NS_THEME_BUTTON_ARROW_DOWN ||
        aWidgetType == NS_THEME_BUTTON_ARROW_NEXT ||
        aWidgetType == NS_THEME_BUTTON_ARROW_PREVIOUS) {

    return false;
  }
  return true;
}

bool
HeadlessThemeGTK::ThemeDrawsFocusForWidget(uint8_t aWidgetType)
{
   if (aWidgetType == NS_THEME_MENULIST ||
       aWidgetType == NS_THEME_BUTTON ||
       aWidgetType == NS_THEME_TREEHEADERCELL) {
    return true;
  }
  return false;
}

bool
HeadlessThemeGTK::ThemeNeedsComboboxDropmarker()
{
  return false;
}


} // namespace widget
} // namespace mozilla
