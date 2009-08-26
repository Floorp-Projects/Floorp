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
#include "nsIAtom.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsMargin.h"
#include "nsILookAndFeel.h"
#include "nsWidgetAtoms.h"

class nsIFrame;
class nsIPresShell;
class nsPresContext;

class nsNativeTheme
{
 protected:

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

  // Returns the content state (hover, focus, etc), see nsIEventStateManager.h
  PRInt32 GetContentState(nsIFrame* aFrame, PRUint8 aWidgetType);

  // Returns whether the widget is already styled by content
  // Normally called from ThemeSupportsWidget to turn off native theming
  // for elements that are already styled.
  PRBool IsWidgetStyled(nsPresContext* aPresContext, nsIFrame* aFrame,
                        PRUint8 aWidgetType);                                              

  // Accessors to widget-specific state information

  // all widgets:
  PRBool IsDisabled(nsIFrame* aFrame) {
    return CheckBooleanAttr(aFrame, nsWidgetAtoms::disabled);
  }

  // RTL chrome direction
  PRBool IsFrameRTL(nsIFrame* aFrame);

  // button:
  PRBool IsDefaultButton(nsIFrame* aFrame) {
    return CheckBooleanAttr(aFrame, nsWidgetAtoms::_default);
  }

  PRBool IsButtonTypeMenu(nsIFrame* aFrame);

  // checkbox:
  PRBool IsChecked(nsIFrame* aFrame) {
    return GetCheckedOrSelected(aFrame, PR_FALSE);
  }

  // radiobutton:
  PRBool IsSelected(nsIFrame* aFrame) {
    return GetCheckedOrSelected(aFrame, PR_TRUE);
  }
  
  PRBool IsFocused(nsIFrame* aFrame) {
    return CheckBooleanAttr(aFrame, nsWidgetAtoms::focused);
  }
  
  // scrollbar button:
  PRInt32 GetScrollbarButtonType(nsIFrame* aFrame);

  // tab:
  PRBool IsSelectedTab(nsIFrame* aFrame) {
    return CheckBooleanAttr(aFrame, nsWidgetAtoms::selected);
  }
  
  PRBool IsNextToSelectedTab(nsIFrame* aFrame, PRInt32 aOffset);
  
  PRBool IsBeforeSelectedTab(nsIFrame* aFrame) {
    return IsNextToSelectedTab(aFrame, -1);
  }
  
  PRBool IsAfterSelectedTab(nsIFrame* aFrame) {
    return IsNextToSelectedTab(aFrame, 1);
  }

  PRBool IsLeftToSelectedTab(nsIFrame* aFrame) {
    return IsFrameRTL(aFrame) ? IsAfterSelectedTab(aFrame) : IsBeforeSelectedTab(aFrame);
  }

  PRBool IsRightToSelectedTab(nsIFrame* aFrame) {
    return IsFrameRTL(aFrame) ? IsBeforeSelectedTab(aFrame) : IsAfterSelectedTab(aFrame);
  }

  // button / toolbarbutton:
  PRBool IsCheckedButton(nsIFrame* aFrame) {
    return CheckBooleanAttr(aFrame, nsWidgetAtoms::checked);
  }

  PRBool IsOpenButton(nsIFrame* aFrame) {
    return CheckBooleanAttr(aFrame, nsWidgetAtoms::open);
  }

  // treeheadercell:
  TreeSortDirection GetTreeSortDirection(nsIFrame* aFrame);
  PRBool IsLastTreeHeaderCell(nsIFrame* aFrame);

  // tab:
  PRBool IsBottomTab(nsIFrame* aFrame);
  PRBool IsFirstTab(nsIFrame* aFrame);
  PRBool IsLastTab(nsIFrame* aFrame);
  
  PRBool IsHorizontal(nsIFrame* aFrame);

  // progressbar:
  PRBool IsIndeterminateProgress(nsIFrame* aFrame);

  PRInt32 GetProgressValue(nsIFrame* aFrame) {
    return CheckIntAttr(aFrame, nsWidgetAtoms::value, 0);
  }
  
  PRInt32 GetProgressMaxValue(nsIFrame* aFrame) {
    return PR_MAX(CheckIntAttr(aFrame, nsWidgetAtoms::max, 100), 1);
  }

  // textfield:
  PRBool IsReadOnly(nsIFrame* aFrame) {
      return CheckBooleanAttr(aFrame, nsWidgetAtoms::readonly);
  }

  // menupopup:
  PRBool IsSubmenu(nsIFrame* aFrame, PRBool* aLeftOfParent);

  nsIPresShell *GetPresShell(nsIFrame* aFrame);
  PRInt32 CheckIntAttr(nsIFrame* aFrame, nsIAtom* aAtom, PRInt32 defaultValue);
  PRBool CheckBooleanAttr(nsIFrame* aFrame, nsIAtom* aAtom);

  PRBool GetCheckedOrSelected(nsIFrame* aFrame, PRBool aCheckSelected);
  PRBool GetIndeterminate(nsIFrame* aFrame);
};
