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

nsMargin nsNativeTheme::sButtonBorderSize(2, 2, 2, 2);
nsMargin nsNativeTheme::sButtonDisabledBorderSize(1, 1, 1, 1);
PRUint8  nsNativeTheme::sButtonActiveBorderStyle = NS_STYLE_BORDER_STYLE_INSET;
PRUint8  nsNativeTheme::sButtonInactiveBorderStyle = NS_STYLE_BORDER_STYLE_OUTSET;
nsILookAndFeel::nsColorID nsNativeTheme::sButtonBorderColorID = nsILookAndFeel::eColor_threedface;
nsILookAndFeel::nsColorID nsNativeTheme::sButtonDisabledBorderColorID = nsILookAndFeel::eColor_threedshadow;
nsILookAndFeel::nsColorID nsNativeTheme::sButtonBGColorID = nsILookAndFeel::eColor_threedface;
nsILookAndFeel::nsColorID nsNativeTheme::sButtonDisabledBGColorID = nsILookAndFeel::eColor_threedface;
nsMargin nsNativeTheme::sTextfieldBorderSize(2, 2, 2, 2);
PRUint8  nsNativeTheme::sTextfieldBorderStyle = NS_STYLE_BORDER_STYLE_INSET;
nsILookAndFeel::nsColorID nsNativeTheme::sTextfieldBorderColorID = nsILookAndFeel::eColor_threedface;
PRBool   nsNativeTheme::sTextfieldBGTransparent = PR_FALSE;
nsILookAndFeel::nsColorID nsNativeTheme::sTextfieldBGColorID = nsILookAndFeel::eColor__moz_field;
nsILookAndFeel::nsColorID nsNativeTheme::sTextfieldDisabledBGColorID = nsILookAndFeel::eColor_threedface;
nsMargin nsNativeTheme::sListboxBorderSize(2, 2, 2, 2);
PRUint8  nsNativeTheme::sListboxBorderStyle = NS_STYLE_BORDER_STYLE_INSET;
nsILookAndFeel::nsColorID nsNativeTheme::sListboxBorderColorID = nsILookAndFeel::eColor_threedface;
PRBool   nsNativeTheme::sListboxBGTransparent = PR_FALSE;
nsILookAndFeel::nsColorID nsNativeTheme::sListboxBGColorID = nsILookAndFeel::eColor__moz_field;
nsILookAndFeel::nsColorID nsNativeTheme::sListboxDisabledBGColorID = nsILookAndFeel::eColor_threedface;

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
    (aWidgetType == NS_THEME_CHECKBOX || aWidgetType == NS_THEME_RADIO) &&
    aFrame->GetContent()->IsNodeOfType(nsINode::eXUL);
  if (isXULCheckboxRadio)
    aFrame = aFrame->GetParent();

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
  if (content->IsNodeOfType(nsINode::eHTML))
    return content->HasAttr(kNameSpaceID_None, aAtom);

  // For XML/XUL elements, an attribute must be equal to the literal
  // string "true" to be counted as true.  An empty string should _not_
  // be counted as true.
  return content->AttrValueIs(kNameSpaceID_None, aAtom,
                              NS_LITERAL_STRING("true"), eCaseMatters);
}

PRInt32
nsNativeTheme::CheckIntAttr(nsIFrame* aFrame, nsIAtom* aAtom)
{
  if (!aFrame)
    return 0;

  nsAutoString attr;
  aFrame->GetContent()->GetAttr(kNameSpaceID_None, aAtom, attr);
  PRInt32 err, value = attr.ToInteger(&err);
  if (NS_FAILED(err))
    return 0;

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

static void
ConvertMarginToAppUnits(const nsMargin &aSource, nsMargin &aDest)
{
  PRInt32 p2a = nsPresContext::AppUnitsPerCSSPixel();
  aDest.top = NSIntPixelsToAppUnits(aSource.top, p2a);
  aDest.left = NSIntPixelsToAppUnits(aSource.left, p2a);
  aDest.bottom = NSIntPixelsToAppUnits(aSource.bottom, p2a);
  aDest.right = NSIntPixelsToAppUnits(aSource.right, p2a);
}

PRBool
nsNativeTheme::IsWidgetStyled(nsPresContext* aPresContext, nsIFrame* aFrame,
                              PRUint8 aWidgetType)
{
  // Check for specific widgets to see if HTML has overridden the style.
  if (aFrame && (aWidgetType == NS_THEME_BUTTON ||
                 aWidgetType == NS_THEME_TEXTFIELD ||
                 aWidgetType == NS_THEME_LISTBOX ||
                 aWidgetType == NS_THEME_DROPDOWN)) {

    if (aFrame->GetContent()->IsNodeOfType(nsINode::eHTML)) {
      nscolor defaultBGColor, defaultBorderColor;
      PRUint8 defaultBorderStyle;
      nsMargin defaultBorderSize;
      PRBool defaultBGTransparent = PR_FALSE;

      nsILookAndFeel *lookAndFeel = aPresContext->LookAndFeel();

      switch (aWidgetType) {
      case NS_THEME_BUTTON:
        if (IsDisabled(aFrame)) {
          ConvertMarginToAppUnits(sButtonDisabledBorderSize, defaultBorderSize);
          defaultBorderStyle = sButtonInactiveBorderStyle;
          lookAndFeel->GetColor(sButtonDisabledBorderColorID,
                                defaultBorderColor);
          lookAndFeel->GetColor(sButtonDisabledBGColorID,
                                defaultBGColor);
        } else {
          PRInt32 contentState = GetContentState(aFrame, aWidgetType);
          ConvertMarginToAppUnits(sButtonBorderSize, defaultBorderSize);
          if (contentState & NS_EVENT_STATE_HOVER &&
              contentState & NS_EVENT_STATE_ACTIVE)
            defaultBorderStyle = sButtonActiveBorderStyle;
          else
            defaultBorderStyle = sButtonInactiveBorderStyle;
          lookAndFeel->GetColor(sButtonBorderColorID,
                                defaultBorderColor);
          lookAndFeel->GetColor(sButtonBGColorID,
                                defaultBGColor);
        }
        break;

      case NS_THEME_TEXTFIELD:
        defaultBorderStyle = sTextfieldBorderStyle;
        ConvertMarginToAppUnits(sTextfieldBorderSize, defaultBorderSize);
        lookAndFeel->GetColor(sTextfieldBorderColorID,
                              defaultBorderColor);
        if (!(defaultBGTransparent = sTextfieldBGTransparent)) {
          if (IsDisabled(aFrame))
            lookAndFeel->GetColor(sTextfieldDisabledBGColorID,
                                  defaultBGColor);
          else
            lookAndFeel->GetColor(sTextfieldBGColorID,
                                  defaultBGColor);
        }
        break;

      case NS_THEME_LISTBOX:
      case NS_THEME_DROPDOWN:
        defaultBorderStyle = sListboxBorderStyle;
        ConvertMarginToAppUnits(sListboxBorderSize, defaultBorderSize);
        lookAndFeel->GetColor(sListboxBorderColorID,
                              defaultBorderColor);
        if (!(defaultBGTransparent = sListboxBGTransparent)) {
          if (IsDisabled(aFrame))
            lookAndFeel->GetColor(sListboxDisabledBGColorID,
                                  defaultBGColor);
          else
            lookAndFeel->GetColor(sListboxBGColorID,
                                  defaultBGColor);
        }
        break;

      default:
        NS_ERROR("nsNativeTheme::IsWidgetStyled widget type not handled");
        return PR_FALSE;
      }

      // Check whether background differs from default
      const nsStyleBackground* ourBG = aFrame->GetStyleBackground();

      if (defaultBGTransparent) {
        if (!(ourBG->mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT))
          return PR_TRUE;
      } else if (ourBG->mBackgroundColor != defaultBGColor ||
                 ourBG->mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT)
        return PR_TRUE;

      if (!(ourBG->mBackgroundFlags & NS_STYLE_BG_IMAGE_NONE))
        return PR_TRUE;

      // Check whether border style or color differs from default
      const nsStyleBorder* ourBorder = aFrame->GetStyleBorder();

      for (PRInt32 i = 0; i < 4; ++i) {
        if (ourBorder->GetBorderStyle(i) != defaultBorderStyle)
          return PR_TRUE;

        PRBool borderFG, borderClear;
        nscolor borderColor;
        ourBorder->GetBorderColor(i, borderColor, borderFG, borderClear);
        if (borderColor != defaultBorderColor || borderClear)
          return PR_TRUE;
      }

      // Check whether border size differs from default
      if (ourBorder->GetBorder() != defaultBorderSize)
        return PR_TRUE;
    }
  }

  return PR_FALSE;
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

  return aFrame->GetContent()->HasAttr(kNameSpaceID_None, nsWidgetAtoms::firsttab);
}

PRBool
nsNativeTheme::IsLastTab(nsIFrame* aFrame)
{
  if (!aFrame)
    return PR_FALSE;

  return aFrame->GetContent()->HasAttr(kNameSpaceID_None, nsWidgetAtoms::lasttab);
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
