/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>  (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// This defines a common base class for nsITheme implementations, to reduce
// code duplication.

#include "prtypes.h"
#include "nsAlgorithm.h"
#include "nsIAtom.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsMargin.h"
#include "nsWidgetAtoms.h"
#include "nsEventStates.h"
#include "nsTArray.h"
#include "nsITimer.h"

class nsIContent;
class nsIFrame;
class nsIPresShell;
class nsPresContext;

class nsNativeTheme : public nsITimerCallback
{
 protected:

  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

  enum ScrollbarButtonType {
    eScrollbarButton_UpTop   = 0,
    eScrollbarButton_Down    = 1 << 0,
    eScrollbarButton_Bottom  = 1 << 1
  };

  enum TreeSortDirection {
    eTreeSortDirection_Descending,
    eTreeSortDirection_Natural,
    eTreeSortDirection_Ascending
  };

  nsNativeTheme();

  // Returns the content state (hover, focus, etc), see nsEventStateManager.h
  nsEventStates GetContentState(nsIFrame* aFrame, PRUint8 aWidgetType);

  // Returns whether the widget is already styled by content
  // Normally called from ThemeSupportsWidget to turn off native theming
  // for elements that are already styled.
  bool IsWidgetStyled(nsPresContext* aPresContext, nsIFrame* aFrame,
                        PRUint8 aWidgetType);                                              

  // Accessors to widget-specific state information

  bool IsDisabled(nsIFrame* aFrame, nsEventStates aEventStates);

  // RTL chrome direction
  bool IsFrameRTL(nsIFrame* aFrame);

  // button:
  bool IsDefaultButton(nsIFrame* aFrame) {
    return CheckBooleanAttr(aFrame, nsWidgetAtoms::_default);
  }

  bool IsButtonTypeMenu(nsIFrame* aFrame);

  // checkbox:
  bool IsChecked(nsIFrame* aFrame) {
    return GetCheckedOrSelected(aFrame, PR_FALSE);
  }

  // radiobutton:
  bool IsSelected(nsIFrame* aFrame) {
    return GetCheckedOrSelected(aFrame, PR_TRUE);
  }
  
  bool IsFocused(nsIFrame* aFrame) {
    return CheckBooleanAttr(aFrame, nsWidgetAtoms::focused);
  }
  
  // scrollbar button:
  PRInt32 GetScrollbarButtonType(nsIFrame* aFrame);

  // tab:
  bool IsSelectedTab(nsIFrame* aFrame) {
    return CheckBooleanAttr(aFrame, nsWidgetAtoms::selected);
  }
  
  bool IsNextToSelectedTab(nsIFrame* aFrame, PRInt32 aOffset);
  
  bool IsBeforeSelectedTab(nsIFrame* aFrame) {
    return IsNextToSelectedTab(aFrame, -1);
  }
  
  bool IsAfterSelectedTab(nsIFrame* aFrame) {
    return IsNextToSelectedTab(aFrame, 1);
  }

  bool IsLeftToSelectedTab(nsIFrame* aFrame) {
    return IsFrameRTL(aFrame) ? IsAfterSelectedTab(aFrame) : IsBeforeSelectedTab(aFrame);
  }

  bool IsRightToSelectedTab(nsIFrame* aFrame) {
    return IsFrameRTL(aFrame) ? IsBeforeSelectedTab(aFrame) : IsAfterSelectedTab(aFrame);
  }

  // button / toolbarbutton:
  bool IsCheckedButton(nsIFrame* aFrame) {
    return CheckBooleanAttr(aFrame, nsWidgetAtoms::checked);
  }

  bool IsSelectedButton(nsIFrame* aFrame) {
    return CheckBooleanAttr(aFrame, nsWidgetAtoms::checked) ||
           CheckBooleanAttr(aFrame, nsWidgetAtoms::selected);
  }

  bool IsOpenButton(nsIFrame* aFrame) {
    return CheckBooleanAttr(aFrame, nsWidgetAtoms::open);
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
  bool IsIndeterminateProgress(nsIFrame* aFrame, nsEventStates aEventStates);
  bool IsVerticalProgress(nsIFrame* aFrame);

  // textfield:
  bool IsReadOnly(nsIFrame* aFrame) {
      return CheckBooleanAttr(aFrame, nsWidgetAtoms::readonly);
  }

  // menupopup:
  bool IsSubmenu(nsIFrame* aFrame, bool* aLeftOfParent);

  // True if it's not a menubar item or menulist item
  bool IsRegularMenuItem(nsIFrame *aFrame);

  bool IsMenuListEditable(nsIFrame *aFrame);

  nsIPresShell *GetPresShell(nsIFrame* aFrame);
  PRInt32 CheckIntAttr(nsIFrame* aFrame, nsIAtom* aAtom, PRInt32 defaultValue);
  bool CheckBooleanAttr(nsIFrame* aFrame, nsIAtom* aAtom);

  bool GetCheckedOrSelected(nsIFrame* aFrame, bool aCheckSelected);
  bool GetIndeterminate(nsIFrame* aFrame);

  bool QueueAnimatedContentForRefresh(nsIContent* aContent,
                                        PRUint32 aMinimumFrameRate);

  nsIFrame* GetAdjacentSiblingFrameWithSameAppearance(nsIFrame* aFrame,
                                                      bool aNextSibling);

 private:
  PRUint32 mAnimatedContentTimeout;
  nsCOMPtr<nsITimer> mAnimatedContentTimer;
  nsAutoTArray<nsCOMPtr<nsIContent>, 20> mAnimatedContentList;
};
