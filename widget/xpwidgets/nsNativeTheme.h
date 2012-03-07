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
#include "nsGkAtoms.h"
#include "nsEventStates.h"
#include "nsTArray.h"
#include "nsITimer.h"
#include "nsClassHashtable.h"
#include "nsIObserver.h"
#include "mozilla/TimeStamp.h"

class nsIContent;
class nsIFrame;
class nsIPresShell;
class nsPresContext;

class nsNativeTheme :
  public nsITimerCallback,
  public nsIObserver
{
 protected:
  typedef mozilla::TimeStamp TimeStamp;
  typedef mozilla::TimeDuration TimeDuration;

  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSIOBSERVER

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
  
  bool IsFocused(nsIFrame* aFrame) {
    return CheckBooleanAttr(aFrame, nsGkAtoms::focused);
  }
  
  // scrollbar button:
  PRInt32 GetScrollbarButtonType(nsIFrame* aFrame);

  // tab:
  bool IsSelectedTab(nsIFrame* aFrame) {
    return CheckBooleanAttr(aFrame, nsGkAtoms::selected);
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
  bool IsIndeterminateProgress(nsIFrame* aFrame, nsEventStates aEventStates);
  bool IsVerticalProgress(nsIFrame* aFrame);

  // textfield:
  bool IsReadOnly(nsIFrame* aFrame) {
      return CheckBooleanAttr(aFrame, nsGkAtoms::readonly);
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

  nsIFrame* GetAdjacentSiblingFrameWithSameAppearance(nsIFrame* aFrame,
                                                      bool aNextSibling);

  // Queue a themed element for a redraw after a set interval.
  bool QueueAnimatedContentForRefresh(nsIContent* aContent,
                                      PRUint32 aMinimumFrameRate);

  /*
   * Simple two phase animations on themed widgets - 'Fades' transition from
   * a base state to a highlighted state and back to the base state, at which
   * point data associated with the fade is freed.
   *
   * Important notes:
   *
   * Consumers are responsible for triggering refresh calls by calling
   * QueueAnimatedContentRefreshForFade on each redraw.
   *
   * Consumers are also responsible for switching fade transitions from
   * FADE_IN/FADE_IN_FINISHED to FADE_OUT through calls to QACRFF. Failing
   * to do so keeps content / fade data stored in mAnimatedFadesList until
   * the content's underlying frame is destroyed or the application closes.
   */

  // Fade states
  typedef enum FadeState {
    FADE_NOTACTIVE = 0,   // Fade state not found, fade complete
    FADE_IN = 1,          // Fading in
    FADE_IN_FINISHED = 2, // Fade-in is finished, waiting for fade-out
    FADE_OUT = 3,         // Fading out
  };

  /*
   * QueueAnimatedContentRefreshForFade - creates a new fade or requests a
   * refresh on an existing fade in progress.
   *
   * aContent          The themed content element the animation is associated
   *                   with.
   * aFadeDirection    The current direction of the fade. Valid values are
   *                   FADE_IN or FADE_OUT.
   * aMinimumFrameRate The minimum frame rate requested (30 is typical). Value
   *                   is passed to QueueAnimatedContentForRefresh to trigger a
   *                   refresh.
   * aMilliseconds     Duration of the fade-in or fade-out transition.
   * aUserData         Generic consumer data storage for state across rendering
   *                   of individual frames. Updated on every call.
   */
  bool
  QueueAnimatedContentRefreshForFade(nsIContent* aContent,
                                     FadeState aFadeDirection,
                                     PRUint32 aMinimumFrameRate,
                                     PRUint32 aMilliseconds,
                                     PRUint32 aUserData = 0);

  // Max ticks returned by FadeData->GetTicks().
  #define TICK_MAX 100.0

  // Internal data structure for storing fade data
  class FadeData
  {
  public:
    /*
     * FadeData()
     * aTimeout  now + duration
     * aState    FADE_IN or FADE_OUT
     * aUserData intial value for user data 
     */
    FadeData(TimeStamp aTimeout, FadeState aState, PRUint32 aUserData) :
      mTimeout(aTimeout),
      mStartTime(TimeStamp::Now()),
      mState(aState),
      mUserData(aUserData) {
    }
    ~FadeData() {}

    /*
     * Reset - resets the to a new timeout value and direction.
     * aTimeout  msec(now) + duration
     * aState    FADE_IN or FADE_OUT
     */
    void Reset(TimeDuration aTimeout, FadeState aState) { 
      NS_ASSERTION((aState == FADE_IN || aState == FADE_OUT),
                   "Bad fade direction.");
      mStartTime = TimeStamp::Now();
      mTimeout = TimeStamp::Now() + aTimeout;
      mState = aState;
    }

    /*
     * GetTicks - returns the number of ticks in this animation where
     * ticks >= 0 && ticks <= TICK_MAX. FADE_IN has increasing ticks,
     * FADE_OUT decreasing.
     */
    PRUint32 GetTicks() {
      TimeStamp now = TimeStamp::Now();
      if (now >= mTimeout) {
        return (mState == FADE_OUT ? 0 : (PRUint32)TICK_MAX);
      }
      TimeDuration diff = now - mStartTime;
      PRUint32 tick =
        (PRUint32)ceil((diff / (mTimeout - mStartTime)) * TICK_MAX);
      // we want ticks to ascend and descend according to the direction.
      if (mState == FADE_OUT) {
        tick = (PRUint32)abs(tick - TICK_MAX);
      }
      return tick;
    }

    /*
     * TimeoutUsed - for fades that have not completes, returns the
     * amount of time used thus far in the current transition in msec.
     */
    TimeDuration TimeoutUsed() {
      TimeDuration used = TimeStamp::Now() - mStartTime;
      TimeDuration totalTime = mTimeout - mStartTime;
      return NS_MIN(used, totalTime);
    }

    /*
     * Misc. data getters/setters
     */
    TimeStamp GetTimeout() { return mTimeout; }
    FadeState GetState() { return mState; }
    void FadeInFinished() { mState = FADE_IN_FINISHED; }
    PRUint32 GetUserData() { return mUserData; }
    void SetUserData(PRUint32 aUserData) { mUserData = aUserData; }

  private:
    TimeStamp mTimeout;
    TimeStamp mStartTime;
    FadeState mState;
    PRUint32 mUserData;
  };

  /*
   * nsNativeTheme fade data helpers
   */

  // Retrieves the FadeData object associated with this content, or null.
  FadeData* GetFade(nsIContent* aContent);
  // Retrieves the current fade state or FADE_NOTACTIVE.
  FadeState GetFadeState(nsIContent* aContent);
  // Retrieves the current tick count for a fade transition or 0. Ticks
  // range from 0 -> TICK_MAX. For FADE_IN transitions ticks increase,
  // for FADE_OUT transitions ticks decrease.
  PRUint32 GetFadeTicks(nsIContent* aContent);
  // Retrieves the alpha value (0->1) corresponding to the current tick
  // count for a fade transition, or 0.
  double GetFadeAlpha(nsIContent* aContent);
  // Get/set consumer data. Valid across each call to QACRFF.
  PRUint32 GetFadeUserData(nsIContent* aContent);
  void SetFadeUserData(nsIContent* aContent, PRUint32 aUserData);
  // Cancel an active fade and free its resources.
  void CancelFade(nsIContent* aContent);
  // Mark a fade as FADE_IN_FINISHED.
  void FinishFadeIn(nsIContent* aContent);

 private:
  nsresult InitFadeList();

  PRUint32 mAnimatedContentTimeout;
  nsCOMPtr<nsITimer> mAnimatedContentTimer;
  // Render refresh list - nsIContent contains the content
  // that will be invalidated when mAnimatedContentTimer fires.
  // Cleared on every call to mAnimatedContentTimer Notify.
  nsAutoTArray<nsCOMPtr<nsIContent>, 20> mAnimatedContentList;
  // Fade list data - nsISupportsHashKey contains the nsIContent
  // associated with an active fade.
  nsClassHashtable<nsISupportsHashKey, FadeData> mAnimatedFadesList;
};
