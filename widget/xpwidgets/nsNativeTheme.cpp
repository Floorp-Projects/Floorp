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
#include "nsEventStateManager.h"
#include "nsString.h"
#include "nsINameSpaceManager.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMXULMenuListElement.h"
#include "nsThemeConstants.h"
#include "nsIComponentManager.h"
#include "nsPIDOMWindow.h"
#include "nsProgressFrame.h"
#include "nsMenuFrame.h"
#include "mozilla/dom/Element.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"

nsNativeTheme::nsNativeTheme()
: mAnimatedContentTimeout(PR_UINT32_MAX)
{
}

NS_IMPL_ISUPPORTS2(nsNativeTheme, nsITimerCallback, nsIObserver)

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

nsEventStates
nsNativeTheme::GetContentState(nsIFrame* aFrame, PRUint8 aWidgetType)
{
  if (!aFrame)
    return nsEventStates();

  bool isXULCheckboxRadio = 
    (aWidgetType == NS_THEME_CHECKBOX ||
     aWidgetType == NS_THEME_RADIO) &&
    aFrame->GetContent()->IsXUL();
  if (isXULCheckboxRadio)
    aFrame = aFrame->GetParent();

  if (!aFrame->GetContent())
    return nsEventStates();

  nsIPresShell *shell = GetPresShell(aFrame);
  if (!shell)
    return nsEventStates();

  nsIContent* frameContent = aFrame->GetContent();
  nsEventStates flags;
  if (frameContent->IsElement()) {
    flags = frameContent->AsElement()->State();
  }
  
  if (isXULCheckboxRadio && aWidgetType == NS_THEME_RADIO) {
    if (IsFocused(aFrame))
      flags |= NS_EVENT_STATE_FOCUS;
  }

  // On Windows and Mac, only draw focus rings if they should be shown. This
  // means that focus rings are only shown once the keyboard has been used to
  // focus something in the window.
#if defined(XP_MACOSX)
  // Mac always draws focus rings for textboxes and lists.
  if (aWidgetType == NS_THEME_TEXTFIELD ||
      aWidgetType == NS_THEME_TEXTFIELD_MULTILINE ||
      aWidgetType == NS_THEME_SEARCHFIELD ||
      aWidgetType == NS_THEME_LISTBOX) {
    return flags;
  }
#endif
#if defined(XP_WIN)
  // On Windows, focused buttons are always drawn as such by the native theme.
  if (aWidgetType == NS_THEME_BUTTON)
    return flags;
#endif    
#if defined(XP_MACOSX) || defined(XP_WIN)
  nsIDocument* doc = aFrame->GetContent()->OwnerDoc();
  nsPIDOMWindow* window = doc->GetWindow();
  if (window && !window->ShouldShowFocusRing())
    flags &= ~NS_EVENT_STATE_FOCUS;
#endif
  
  return flags;
}

bool
nsNativeTheme::CheckBooleanAttr(nsIFrame* aFrame, nsIAtom* aAtom)
{
  if (!aFrame)
    return false;

  nsIContent* content = aFrame->GetContent();
  if (!content)
    return false;

  if (content->IsHTML())
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

bool
nsNativeTheme::GetCheckedOrSelected(nsIFrame* aFrame, bool aCheckSelected)
{
  if (!aFrame)
    return false;

  nsIContent* content = aFrame->GetContent();

  if (content->IsXUL()) {
    // For a XUL checkbox or radio button, the state of the parent determines
    // the checked state
    aFrame = aFrame->GetParent();
  } else {
    // Check for an HTML input element
    nsCOMPtr<nsIDOMHTMLInputElement> inputElt = do_QueryInterface(content);
    if (inputElt) {
      bool checked;
      inputElt->GetChecked(&checked);
      return checked;
    }
  }

  return CheckBooleanAttr(aFrame, aCheckSelected ? nsGkAtoms::selected
                                                 : nsGkAtoms::checked);
}

bool
nsNativeTheme::IsButtonTypeMenu(nsIFrame* aFrame)
{
  if (!aFrame)
    return false;

  nsIContent* content = aFrame->GetContent();
  return content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                              NS_LITERAL_STRING("menu"), eCaseMatters);
}

bool
nsNativeTheme::IsPressedButton(nsIFrame* aFrame)
{
  nsEventStates eventState = GetContentState(aFrame, NS_THEME_TOOLBAR_BUTTON);
  if (IsDisabled(aFrame, eventState))
    return false;

  return IsOpenButton(aFrame) ||
         eventState.HasAllStates(NS_EVENT_STATE_ACTIVE | NS_EVENT_STATE_HOVER);
}


bool
nsNativeTheme::GetIndeterminate(nsIFrame* aFrame)
{
  if (!aFrame)
    return false;

  nsIContent* content = aFrame->GetContent();

  if (content->IsXUL()) {
    // For a XUL checkbox or radio button, the state of the parent determines
    // the state
    return CheckBooleanAttr(aFrame->GetParent(), nsGkAtoms::indeterminate);
  }

  // Check for an HTML input element
  nsCOMPtr<nsIDOMHTMLInputElement> inputElt = do_QueryInterface(content);
  if (inputElt) {
    bool indeterminate;
    inputElt->GetIndeterminate(&indeterminate);
    return indeterminate;
  }

  return false;
}

bool
nsNativeTheme::IsWidgetStyled(nsPresContext* aPresContext, nsIFrame* aFrame,
                              PRUint8 aWidgetType)
{
  // Check for specific widgets to see if HTML has overridden the style.
  if (!aFrame)
    return false;

  // Resizers have some special handling, dependent on whether in a scrollable
  // container or not. If so, use the scrollable container's to determine
  // whether the style is overriden instead of the resizer. This allows a
  // non-native transparent resizer to be used instead. Otherwise, we just
  // fall through and return false.
  if (aWidgetType == NS_THEME_RESIZER) {
    nsIFrame* parentFrame = aFrame->GetParent();
    if (parentFrame && parentFrame->GetType() == nsGkAtoms::scrollFrame) {
      // if the parent is a scrollframe, the resizer should be native themed
      // only if the scrollable area doesn't override the widget style.
      parentFrame = parentFrame->GetParent();
      if (parentFrame) {
        return IsWidgetStyled(aPresContext, parentFrame,
                              parentFrame->GetStyleDisplay()->mAppearance);
      }
    }
  }

  /**
   * Progress bar appearance should be the same for the bar and the container
   * frame. nsProgressFrame owns the logic and will tell us what we should do.
   */
  if (aWidgetType == NS_THEME_PROGRESSBAR_CHUNK ||
      aWidgetType == NS_THEME_PROGRESSBAR) {
    nsProgressFrame* progressFrame = do_QueryFrame(aWidgetType == NS_THEME_PROGRESSBAR_CHUNK
                                       ? aFrame->GetParent() : aFrame);
    if (progressFrame) {
      return !progressFrame->ShouldUseNativeStyle();
    }
  }

  return (aWidgetType == NS_THEME_BUTTON ||
          aWidgetType == NS_THEME_TEXTFIELD ||
          aWidgetType == NS_THEME_TEXTFIELD_MULTILINE ||
          aWidgetType == NS_THEME_LISTBOX ||
          aWidgetType == NS_THEME_DROPDOWN) &&
         aFrame->GetContent()->IsHTML() &&
         aPresContext->HasAuthorSpecifiedRules(aFrame,
                                               NS_AUTHOR_SPECIFIED_BORDER |
                                               NS_AUTHOR_SPECIFIED_BACKGROUND);
}

bool
nsNativeTheme::IsDisabled(nsIFrame* aFrame, nsEventStates aEventStates)
{
  if (!aFrame) {
    return false;
  }

  nsIContent* content = aFrame->GetContent();
  if (!content) {
    return false;
  }

  if (content->IsHTML()) {
    return aEventStates.HasState(NS_EVENT_STATE_DISABLED);
  }

  // For XML/XUL elements, an attribute must be equal to the literal
  // string "true" to be counted as true.  An empty string should _not_
  // be counted as true.
  return content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled,
                              NS_LITERAL_STRING("true"), eCaseMatters);
}

bool
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
    {&nsGkAtoms::scrollbarDownBottom, &nsGkAtoms::scrollbarDownTop,
     &nsGkAtoms::scrollbarUpBottom, &nsGkAtoms::scrollbarUpTop,
     nsnull};

  switch (aFrame->GetContent()->FindAttrValueIn(kNameSpaceID_None,
                                                nsGkAtoms::sbattr,
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
  if (!aFrame || !aFrame->GetContent())
    return eTreeSortDirection_Natural;

  static nsIContent::AttrValuesArray strings[] =
    {&nsGkAtoms::descending, &nsGkAtoms::ascending, nsnull};
  switch (aFrame->GetContent()->FindAttrValueIn(kNameSpaceID_None,
                                                nsGkAtoms::sortDirection,
                                                strings, eCaseMatters)) {
    case 0: return eTreeSortDirection_Descending;
    case 1: return eTreeSortDirection_Ascending;
  }

  return eTreeSortDirection_Natural;
}

bool
nsNativeTheme::IsLastTreeHeaderCell(nsIFrame* aFrame)
{
  if (!aFrame)
    return false;

  // A tree column picker is always the last header cell.
  if (aFrame->GetContent()->Tag() == nsGkAtoms::treecolpicker)
    return true;

  // Find the parent tree.
  nsIContent* parent = aFrame->GetContent()->GetParent();
  while (parent && parent->Tag() != nsGkAtoms::tree) {
    parent = parent->GetParent();
  }

  // If the column picker is visible, this can't be the last column.
  if (parent && !parent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::hidecolumnpicker,
                                     NS_LITERAL_STRING("true"), eCaseMatters))
    return false;

  while ((aFrame = aFrame->GetNextSibling())) {
    if (aFrame->GetRect().width > 0)
      return false;
  }
  return true;
}

// tab:
bool
nsNativeTheme::IsBottomTab(nsIFrame* aFrame)
{
  if (!aFrame)
    return false;

  nsAutoString classStr;
  aFrame->GetContent()->GetAttr(kNameSpaceID_None, nsGkAtoms::_class, classStr);
  return !classStr.IsEmpty() && classStr.Find("tab-bottom") != kNotFound;
}

bool
nsNativeTheme::IsFirstTab(nsIFrame* aFrame)
{
  if (!aFrame)
    return false;

  nsIFrame* first = aFrame->GetParent()->GetFirstPrincipalChild();
  while (first) {
    if (first->GetRect().width > 0 && first->GetContent()->Tag() == nsGkAtoms::tab)
      return (first == aFrame);
    first = first->GetNextSibling();
  }
  return false;
}

bool
nsNativeTheme::IsHorizontal(nsIFrame* aFrame)
{
  if (!aFrame)
    return false;
    
  return !aFrame->GetContent()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::orient,
                                            nsGkAtoms::vertical, 
                                            eCaseMatters);
}

bool
nsNativeTheme::IsNextToSelectedTab(nsIFrame* aFrame, PRInt32 aOffset)
{
  if (!aFrame)
    return false;

  if (aOffset == 0)
    return IsSelectedTab(aFrame);

  PRInt32 thisTabIndex = -1, selectedTabIndex = -1;

  nsIFrame* currentTab = aFrame->GetParent()->GetFirstPrincipalChild();
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
    return false;

  return (thisTabIndex - selectedTabIndex == aOffset);
}

// progressbar:
bool
nsNativeTheme::IsIndeterminateProgress(nsIFrame* aFrame,
                                       nsEventStates aEventStates)
{
  if (!aFrame || !aFrame->GetContent())
    return false;

  if (aFrame->GetContent()->IsHTML(nsGkAtoms::progress)) {
    return aEventStates.HasState(NS_EVENT_STATE_INDETERMINATE);
  }

  return aFrame->GetContent()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::mode,
                                           NS_LITERAL_STRING("undetermined"),
                                           eCaseMatters);
}

bool
nsNativeTheme::IsVerticalProgress(nsIFrame* aFrame)
{
  return aFrame &&
         aFrame->GetStyleDisplay()->mOrient == NS_STYLE_ORIENT_VERTICAL;
}

// menupopup:
bool
nsNativeTheme::IsSubmenu(nsIFrame* aFrame, bool* aLeftOfParent)
{
  if (!aFrame)
    return false;

  nsIContent* parentContent = aFrame->GetContent()->GetParent();
  if (!parentContent || parentContent->Tag() != nsGkAtoms::menu)
    return false;

  nsIFrame* parent = aFrame;
  while ((parent = parent->GetParent())) {
    if (parent->GetContent() == parentContent) {
      if (aLeftOfParent) {
        nsIntRect selfBounds, parentBounds;
        aFrame->GetNearestWidget()->GetScreenBounds(selfBounds);
        parent->GetNearestWidget()->GetScreenBounds(parentBounds);
        *aLeftOfParent = selfBounds.x < parentBounds.x;
      }
      return true;
    }
  }

  return false;
}

bool
nsNativeTheme::IsRegularMenuItem(nsIFrame *aFrame)
{
  nsMenuFrame *menuFrame = do_QueryFrame(aFrame);
  return !(menuFrame && (menuFrame->IsOnMenuBar() ||
                         menuFrame->GetParentMenuListType() != eNotMenuList));
}

bool
nsNativeTheme::IsMenuListEditable(nsIFrame *aFrame)
{
  bool isEditable = false;
  nsCOMPtr<nsIDOMXULMenuListElement> menulist = do_QueryInterface(aFrame->GetContent());
  if (menulist)
    menulist->GetEditable(&isEditable);
  return isEditable;
}

bool
nsNativeTheme::QueueAnimatedContentForRefresh(nsIContent* aContent,
                                              PRUint32 aMinimumFrameRate)
{
  NS_ASSERTION(aContent, "Null pointer!");
  NS_ASSERTION(aMinimumFrameRate, "aMinimumFrameRate must be non-zero!");
  NS_ASSERTION(aMinimumFrameRate <= 1000,
               "aMinimumFrameRate must be less than 1000!");

  PRUint32 timeout = 1000 / aMinimumFrameRate;
  timeout = NS_MIN(mAnimatedContentTimeout, timeout);

  if (!mAnimatedContentTimer) {
    mAnimatedContentTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
    NS_ENSURE_TRUE(mAnimatedContentTimer, false);
  }

  if (mAnimatedContentList.IsEmpty() || timeout != mAnimatedContentTimeout) {
    nsresult rv;
    if (!mAnimatedContentList.IsEmpty()) {
      rv = mAnimatedContentTimer->Cancel();
      NS_ENSURE_SUCCESS(rv, false);
    }

    rv = mAnimatedContentTimer->InitWithCallback(this, timeout,
                                                 nsITimer::TYPE_ONE_SHOT);
    NS_ENSURE_SUCCESS(rv, false);

    mAnimatedContentTimeout = timeout;
  }

  if (!mAnimatedContentList.AppendElement(aContent)) {
    NS_WARNING("Out of memory!");
    return false;
  }

  return true;
}

inline bool
IsFadeIn(nsNativeTheme::FadeState aState)
{
  return (aState == nsNativeTheme::FADE_IN ||
          aState == nsNativeTheme::FADE_IN_FINISHED);
}

inline bool
IsFadeOut(nsNativeTheme::FadeState aState)
{
  return (aState == nsNativeTheme::FADE_OUT);
}

bool
nsNativeTheme::QueueAnimatedContentRefreshForFade(nsIContent* aContent,
                                                  FadeState aFadeDirection,
                                                  PRUint32 aMinimumFrameRate,
                                                  PRUint32 aMilliseconds,
                                                  PRUint32 aUserData)
{
  NS_ASSERTION(aContent, "Null pointer!");
  NS_ASSERTION((aFadeDirection == FADE_IN ||
                aFadeDirection == FADE_OUT), "Bad initial fade direction.");

  // Initialize our hash table and setup an observer for freeing its contents
  // on shutdown.
  if (NS_FAILED(InitFadeList()))
    return false;

  // Note, QueueAnimatedContentForRefresh failures in here can result in
  // content getting stuck in mAnimatedFadesList until shutdown, so we
  // warn loudly. Generally this should never happen.

  FadeData* pFade = mAnimatedFadesList.Get(aContent);
  if (pFade) {
    // Update the user data
    pFade->SetUserData(aUserData);

    // Check for direction changes and update our fade data accordingly.
    if (IsFadeIn(pFade->GetState()) != IsFadeIn(aFadeDirection)) {
      if (pFade->GetState() != FADE_IN_FINISHED) {
        // The amount of time we spent getting here equals the amount of
        // time we spend getting back out.
        pFade->Reset(pFade->TimeoutUsed(), aFadeDirection);
      } else {
        // Reset to transition timeout passed in.
        //PRUint32 timeout =
        //  PR_IntervalToMilliseconds(PR_IntervalNow()) + aMilliseconds;
        //pFade->Reset(timeout, aFadeDirection);
        pFade->Reset(TimeDuration::FromMilliseconds(aMilliseconds),
                     aFadeDirection);
      }
    }

    // Check for a timeout
    if (pFade->GetTimeout() < TimeStamp::Now()) {
      // If timed out and it's a fade up, set state to finished. We keep the
      // fade data around until a corresponding fade out completes or the
      // underlying frame is destroyed.
      if (IsFadeIn(pFade->GetState())) {
        pFade->FadeInFinished();
        // Create a heartbeat (1 sec) animation timer so if the underlying
        // frame is destroyed, Notify will free the content.
        if (!QueueAnimatedContentForRefresh(aContent, 1)) {
          NS_WARNING("QueueAnimatedContentForRefresh failed???");
          return false;
        }
      } else if (IsFadeOut(pFade->GetState())) {
        // If timed out and it's a fade out, clear it, we're done.
        mAnimatedFadesList.Remove(aContent);
        // Fire one last time to get the base graphic painted.
        if (!QueueAnimatedContentForRefresh(aContent, aMinimumFrameRate)) {
          NS_WARNING("QueueAnimatedContentForRefresh failed???");
          return false;
        }
      }
    } else {
      // fading..
      if (!QueueAnimatedContentForRefresh(aContent, aMinimumFrameRate)) {
        NS_WARNING("QueueAnimatedContentForRefresh failed???");
        return false;
      }
    }
    return true;
  }

  // If we don't have a fade put together a FadeData, store it in
  // mAnimatedFadesList, and kick things off.
  TimeStamp timeout = TimeStamp::Now() +
    TimeDuration::FromMilliseconds(aMilliseconds);
  nsAutoPtr<FadeData> newFade(new FadeData(timeout, aFadeDirection, aUserData));
  if (!newFade) {
    NS_WARNING("Out of memory!");
    return false;
  }
  // Call QueueAnimatedContentForRefresh to kick off the fade animation.
  if (!QueueAnimatedContentForRefresh(aContent, aMinimumFrameRate)) {
    NS_WARNING("QueueAnimatedContentForRefresh failed???");
    return false;
  }
  mAnimatedFadesList.Put(aContent, newFade);
  newFade.forget();

  return true;
}

// mAnimatedFadesList management

nsresult
nsNativeTheme::InitFadeList()
{
  if (mAnimatedFadesList.IsInitialized())
    return NS_OK;
  if (!mAnimatedFadesList.Init())
    return NS_ERROR_UNEXPECTED;
  nsCOMPtr<nsIObserverService> obsSvc =
    mozilla::services::GetObserverService();
  nsresult rv = NS_ERROR_UNEXPECTED;
  if (obsSvc) {
    rv = obsSvc->AddObserver(this, NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID, false);
  }
  return rv;
}

NS_IMETHODIMP
nsNativeTheme::Observe(nsISupports* aSubject, const char* aTopic,
                       const PRUnichar* aData)
{
  if (strcmp(aTopic, NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID) == 0) {
    mAnimatedFadesList.Clear();
    nsCOMPtr<nsIObserverService> obsSvc =
      mozilla::services::GetObserverService();
    nsresult rv = NS_ERROR_UNEXPECTED;
    if (obsSvc) {
      rv = obsSvc->RemoveObserver(this, NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID);
    }
    NS_ASSERTION(NS_SUCCEEDED(rv),
      "nsNativeTheme RemoveObserver failed, this may cause a leak.");
  }
  return NS_OK;
}

// mAnimatedContentTimer callback for QueueAnimatedContentForRefresh

NS_IMETHODIMP
nsNativeTheme::Notify(nsITimer* aTimer)
{
  NS_ASSERTION(aTimer == mAnimatedContentTimer, "Wrong timer!");

  // XXX Assumes that calling nsIFrame::Invalidate won't reenter
  //     QueueAnimatedContentForRefresh.

  PRUint32 count = mAnimatedContentList.Length();
  for (PRUint32 index = 0; index < count; index++) {
    nsIFrame* frame = mAnimatedContentList[index]->GetPrimaryFrame();
    if (frame) {
      frame->InvalidateOverflowRect();
    } else {
      // If this content has fade data associated with it, and the
      // frame has gone away, free the data and cancel the fade.
      if (mAnimatedFadesList.IsInitialized()) {
        mAnimatedFadesList.Remove(mAnimatedContentList[index]);
      }
    }
  }

  mAnimatedContentList.Clear();
  mAnimatedContentTimeout = PR_UINT32_MAX;

  return NS_OK;
}

// Fade helpers

nsNativeTheme::FadeData*
nsNativeTheme::GetFade(nsIContent* aContent)
{
  if (!aContent || !mAnimatedFadesList.IsInitialized())
    return nsnull;
  return mAnimatedFadesList.Get(reinterpret_cast<nsISupports*>(aContent));
}

nsNativeTheme::FadeState
nsNativeTheme::GetFadeState(nsIContent* aContent)
{
  FadeData* pFade = GetFade(aContent);
  if (!pFade)
    return FADE_NOTACTIVE;
  return pFade->GetState();
}

PRUint32
nsNativeTheme::GetFadeTicks(nsIContent* aContent)
{
  FadeData* pFade = GetFade(aContent);
  if (!pFade)
    return 0;
  return pFade->GetTicks();
}

double
nsNativeTheme::GetFadeAlpha(nsIContent* aContent)
{
  return ((double)GetFadeTicks(aContent))/TICK_MAX;
}

PRUint32
nsNativeTheme::GetFadeUserData(nsIContent* aContent)
{
  FadeData* pFade = GetFade(aContent);
  if (!pFade)
    return 0;
  return pFade->GetUserData();
}

void
nsNativeTheme::SetFadeUserData(nsIContent* aContent, PRUint32 aUserData)
{
  FadeData* pFade = GetFade(aContent);
  if (pFade) {
    pFade->SetUserData(aUserData);
  }
}

void
nsNativeTheme::CancelFade(nsIContent* aContent)
{
  if (aContent && mAnimatedFadesList.IsInitialized()) {
    mAnimatedFadesList.Remove(reinterpret_cast<nsISupports*>(aContent));
  }
}

void
nsNativeTheme::FinishFadeIn(nsIContent* aContent)
{
  FadeData* pFade = GetFade(aContent);
  if (pFade) {
    pFade->FadeInFinished();
  }
}

nsIFrame*
nsNativeTheme::GetAdjacentSiblingFrameWithSameAppearance(nsIFrame* aFrame,
                                                         bool aNextSibling)
{
  if (!aFrame)
    return nsnull;

  // Find the next visible sibling.
  nsIFrame* sibling = aFrame;
  do {
    sibling = aNextSibling ? sibling->GetNextSibling() : sibling->GetPrevSibling();
  } while (sibling && sibling->GetRect().width == 0);

  // Check same appearance and adjacency.
  if (!sibling ||
      sibling->GetStyleDisplay()->mAppearance != aFrame->GetStyleDisplay()->mAppearance ||
      (sibling->GetRect().XMost() != aFrame->GetRect().x &&
       aFrame->GetRect().XMost() != sibling->GetRect().x))
    return nsnull;
  return sibling;
}

