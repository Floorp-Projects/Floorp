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

#include "nsNativeTheme.h"
#include "nsIWidget.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIEventStateManager.h"
#include "nsString.h"
#include "nsINameSpaceManager.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsILookAndFeel.h"
#include "nsThemeConstants.h"
#include "nsIComponentManager.h"
#include "nsIDOMNSHTMLInputElement.h"

nsNativeTheme::nsNativeTheme()
{
}

nsIPresShell *
nsNativeTheme::GetPresShell(nsIFrame* aFrame)
{
  if (!aFrame)
    return nsnull;

  // this is a workaround for the egcs 1.1.2 not inliningg
  // aFrame->GetPresContext(), which causes an undefined symbol
  nsPresContext *context = aFrame->GetStyleContext()->GetRuleNode()->GetPresContext();
  return context ? context->GetPresShell() : nsnull;
}

PRInt32
nsNativeTheme::GetContentState(nsIFrame* aFrame, PRUint8 aWidgetType)
{
  if (!aFrame)
    return 0;

  PRBool isXULCheckboxRadio = 
    (aWidgetType == NS_THEME_CHECKBOX ||
     aWidgetType == NS_THEME_RADIO) &&
    aFrame->GetContent()->IsNodeOfType(nsINode::eXUL);
  if (isXULCheckboxRadio)
    aFrame = aFrame->GetParent();

  if (!aFrame->GetContent())
    return 0;

  nsIPresShell *shell = GetPresShell(aFrame);
  if (!shell)
    return 0;

  PRInt32 flags = 0;
  shell->GetPresContext()->EventStateManager()->GetContentState(aFrame->GetContent(), flags);
  
  if (isXULCheckboxRadio && aWidgetType == NS_THEME_RADIO) {
    if (IsFocused(aFrame))
      flags |= NS_EVENT_STATE_FOCUS;
  }
  
  return flags;
}

PRBool
nsNativeTheme::CheckBooleanAttr(nsIFrame* aFrame, nsIAtom* aAtom)
{
  if (!aFrame)
    return PR_FALSE;

  nsIContent* content = aFrame->GetContent();
  if (!content)
    return PR_FALSE;

  if (content->IsNodeOfType(nsINode::eHTML))
    return content->HasAttr(kNameSpaceID_None, aAtom);

  // For XML/XUL elements, an attribute must be equal to the literal
  // string "true" to be counted as true.  An empty string should _not_
  // be counted as true.
  return content->AttrValueIs(kNameSpaceID_None, aAtom,
                              NS_LITERAL_STRING("true"), eCaseMatters);
}

PRInt32
nsNativeTheme::CheckIntAttr(nsIFrame* aFrame, nsIAtom* aAtom, PRInt32 defaultValue)
{
  if (!aFrame)
    return defaultValue;

  nsAutoString attr;
  aFrame->GetContent()->GetAttr(kNameSpaceID_None, aAtom, attr);
  PRInt32 err, value = attr.ToInteger(&err);
  if (attr.IsEmpty() || NS_FAILED(err))
    return defaultValue;

  return value;
}

PRBool
nsNativeTheme::GetCheckedOrSelected(nsIFrame* aFrame, PRBool aCheckSelected)
{
  if (!aFrame)
    return PR_FALSE;

  nsIContent* content = aFrame->GetContent();

  if (content->IsNodeOfType(nsINode::eXUL)) {
    // For a XUL checkbox or radio button, the state of the parent determines
    // the checked state
    aFrame = aFrame->GetParent();
  } else {
    // Check for an HTML input element
    nsCOMPtr<nsIDOMHTMLInputElement> inputElt = do_QueryInterface(content);
    if (inputElt) {
      PRBool checked;
      inputElt->GetChecked(&checked);
      return checked;
    }
  }

  return CheckBooleanAttr(aFrame, aCheckSelected ? nsWidgetAtoms::selected
                                                 : nsWidgetAtoms::checked);
}

PRBool
nsNativeTheme::IsButtonTypeMenu(nsIFrame* aFrame)
{
  if (!aFrame)
    return PR_FALSE;

  nsIContent* content = aFrame->GetContent();
  return content->AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::type,
                              NS_LITERAL_STRING("menu"), eCaseMatters);
}

PRBool
nsNativeTheme::GetIndeterminate(nsIFrame* aFrame)
{
  if (!aFrame)
    return PR_FALSE;

  nsIContent* content = aFrame->GetContent();

  if (content->IsNodeOfType(nsINode::eXUL)) {
    // For a XUL checkbox or radio button, the state of the parent determines
    // the state
    return CheckBooleanAttr(aFrame->GetParent(), nsWidgetAtoms::indeterminate);
  }

  // Check for an HTML input element
  nsCOMPtr<nsIDOMNSHTMLInputElement> inputElt = do_QueryInterface(content);
  if (inputElt) {
    PRBool indeterminate;
    inputElt->GetIndeterminate(&indeterminate);
    return indeterminate;
  }

  return PR_FALSE;
}

PRBool
nsNativeTheme::IsWidgetStyled(nsPresContext* aPresContext, nsIFrame* aFrame,
                              PRUint8 aWidgetType)
{
  // Check for specific widgets to see if HTML has overridden the style.
  return aFrame &&
         (aWidgetType == NS_THEME_BUTTON ||
          aWidgetType == NS_THEME_TEXTFIELD ||
          aWidgetType == NS_THEME_TEXTFIELD_MULTILINE ||
          aWidgetType == NS_THEME_LISTBOX ||
          aWidgetType == NS_THEME_DROPDOWN) &&
         aFrame->GetContent()->IsNodeOfType(nsINode::eHTML) &&
         aPresContext->HasAuthorSpecifiedRules(aFrame,
                                               NS_AUTHOR_SPECIFIED_BORDER |
                                               NS_AUTHOR_SPECIFIED_BACKGROUND);
}

PRBool
nsNativeTheme::IsFrameRTL(nsIFrame* aFrame)
{
  return aFrame && aFrame->GetStyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL;
}

// scrollbar button:
PRInt32
nsNativeTheme::GetScrollbarButtonType(nsIFrame* aFrame)
{
  if (!aFrame)
    return 0;

  static nsIContent::AttrValuesArray strings[] =
    {&nsWidgetAtoms::scrollbarDownBottom, &nsWidgetAtoms::scrollbarDownTop,
     &nsWidgetAtoms::scrollbarUpBottom, &nsWidgetAtoms::scrollbarUpTop,
     nsnull};

  switch (aFrame->GetContent()->FindAttrValueIn(kNameSpaceID_None,
                                                nsWidgetAtoms::sbattr,
                                                strings, eCaseMatters)) {
    case 0: return eScrollbarButton_Down | eScrollbarButton_Bottom;
    case 1: return eScrollbarButton_Down;
    case 2: return eScrollbarButton_Bottom;
    case 3: return eScrollbarButton_UpTop;
  }

  return 0;
}

// treeheadercell:
nsNativeTheme::TreeSortDirection
nsNativeTheme::GetTreeSortDirection(nsIFrame* aFrame)
{
  if (!aFrame)
    return eTreeSortDirection_Natural;

  static nsIContent::AttrValuesArray strings[] =
    {&nsWidgetAtoms::descending, &nsWidgetAtoms::ascending, nsnull};
  switch (aFrame->GetContent()->FindAttrValueIn(kNameSpaceID_None,
                                                nsWidgetAtoms::sortdirection,
                                                strings, eCaseMatters)) {
    case 0: return eTreeSortDirection_Descending;
    case 1: return eTreeSortDirection_Ascending;
  }

  return eTreeSortDirection_Natural;
}

PRBool
nsNativeTheme::IsLastTreeHeaderCell(nsIFrame* aFrame)
{
  if (!aFrame)
    return PR_FALSE;

  // A tree column picker is always the last header cell.
  if (aFrame->GetContent()->Tag() == nsWidgetAtoms::treecolpicker)
    return PR_TRUE;

  // Find the parent tree.
  nsIContent* parent = aFrame->GetContent()->GetParent();
  while (parent && parent->Tag() != nsWidgetAtoms::tree) {
    parent = parent->GetParent();
  }

  // If the column picker is visible, this can't be the last column.
  if (parent && !parent->AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::hidecolumnpicker,
                                     NS_LITERAL_STRING("true"), eCaseMatters))
    return PR_FALSE;

  while ((aFrame = aFrame->GetNextSibling())) {
    if (aFrame->GetRect().width > 0)
      return PR_FALSE;
  }
  return PR_TRUE;
}

// tab:
PRBool
nsNativeTheme::IsBottomTab(nsIFrame* aFrame)
{
  if (!aFrame)
    return PR_FALSE;

  nsAutoString classStr;
  aFrame->GetContent()->GetAttr(kNameSpaceID_None, nsWidgetAtoms::_class, classStr);
  return !classStr.IsEmpty() && classStr.Find("tab-bottom") != kNotFound;
}

PRBool
nsNativeTheme::IsFirstTab(nsIFrame* aFrame)
{
  if (!aFrame)
    return PR_FALSE;

  nsIFrame* first = aFrame->GetParent()->GetFirstChild(nsnull);
  while (first) {
    if (first->GetRect().width > 0 && first->GetContent()->Tag() == nsWidgetAtoms::tab)
      return (first == aFrame);
    first = first->GetNextSibling();
  }
  return PR_FALSE;
}

PRBool
nsNativeTheme::IsLastTab(nsIFrame* aFrame)
{
  if (!aFrame)
    return PR_FALSE;

  while ((aFrame = aFrame->GetNextSibling())) {
    if (aFrame->GetRect().width > 0 && aFrame->GetContent()->Tag() == nsWidgetAtoms::tab)
      return PR_FALSE;
  }
  return PR_TRUE;
}

PRBool
nsNativeTheme::IsHorizontal(nsIFrame* aFrame)
{
  if (!aFrame)
    return PR_FALSE;
    
  return !aFrame->GetContent()->AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::orient,
                                            nsWidgetAtoms::vertical, 
                                            eCaseMatters);
}

PRBool
nsNativeTheme::IsNextToSelectedTab(nsIFrame* aFrame, PRInt32 aOffset)
{
  if (!aFrame)
    return PR_FALSE;

  if (aOffset == 0)
    return IsSelectedTab(aFrame);

  PRInt32 thisTabIndex = -1, selectedTabIndex = -1;

  nsIFrame* currentTab = aFrame->GetParent()->GetFirstChild(NULL);
  for (PRInt32 i = 0; currentTab; currentTab = currentTab->GetNextSibling()) {
    if (currentTab->GetRect().width == 0)
      continue;
    if (aFrame == currentTab)
      thisTabIndex = i;
    if (IsSelectedTab(currentTab))
      selectedTabIndex = i;
    ++i;
  }

  if (thisTabIndex == -1 || selectedTabIndex == -1)
    return PR_FALSE;

  return (thisTabIndex - selectedTabIndex == aOffset);
}

// progressbar:
PRBool
nsNativeTheme::IsIndeterminateProgress(nsIFrame* aFrame)
{
  if (!aFrame)
    return PR_FALSE;

  return aFrame->GetContent()->AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::mode,
                                           NS_LITERAL_STRING("undetermined"),
                                           eCaseMatters);
}

// menupopup:
PRBool
nsNativeTheme::IsSubmenu(nsIFrame* aFrame, PRBool* aLeftOfParent)
{
  if (!aFrame)
    return PR_FALSE;

  nsIContent* parentContent = aFrame->GetContent()->GetParent();
  if (!parentContent || parentContent->Tag() != nsWidgetAtoms::menu)
    return PR_FALSE;

  nsIFrame* parent = aFrame;
  while ((parent = parent->GetParent())) {
    if (parent->GetContent() == parentContent) {
      if (aLeftOfParent) {
        nsIntRect selfBounds, parentBounds;
        aFrame->GetWindow()->GetScreenBounds(selfBounds);
        parent->GetWindow()->GetScreenBounds(parentBounds);
        *aLeftOfParent = selfBounds.x < parentBounds.x;
      }
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}
