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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Aaron Leventhal (aaronl@netscape.com)
 *   Blake Ross      (blake@cs.stanford.edu)
 *   Masayuki Nakano (masayuki@d-toybox.com)
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

#include "nsISelectionController.h"
#include "nsIController.h"
#include "nsIControllers.h"
#include "nsIObserver.h"
#include "nsUnicharUtils.h"
#include "nsIFind.h"
#include "nsIWebBrowserFind.h"
#include "nsWeakReference.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsISelection.h"
#include "nsIDOMRange.h"
#include "nsIDocShellTreeItem.h"
#include "nsITypeAheadFind.h"
#include "nsISupportsArray.h"
#include "nsISound.h"

#define TYPEAHEADFIND_NOTFOUND_WAV_URL \
        "chrome://global/content/notfound.wav"

class nsTypeAheadFind : public nsITypeAheadFind,
                        public nsIObserver,
                        public nsSupportsWeakReference
{
public:
  nsTypeAheadFind();
  virtual ~nsTypeAheadFind();

  NS_DECL_ISUPPORTS
  NS_DECL_NSITYPEAHEADFIND
  NS_DECL_NSIOBSERVER

protected:
  nsresult PrefsReset();

  void SaveFind();
  void PlayNotFoundSound(); 
  nsresult GetWebBrowserFind(nsIDocShell *aDocShell,
                             nsIWebBrowserFind **aWebBrowserFind);

  void RangeStartsInsideLink(nsIDOMRange *aRange, nsIPresShell *aPresShell, 
                             PRBool *aIsInsideLink, PRBool *aIsStartingLink);

  void GetSelection(nsIPresShell *aPresShell, nsISelectionController **aSelCon, 
                    nsISelection **aDomSel);
  PRBool IsRangeVisible(nsIPresShell *aPresShell, nsPresContext *aPresContext,
                        nsIDOMRange *aRange, PRBool aMustBeVisible, 
                        PRBool aGetTopVisibleLeaf, nsIDOMRange **aNewRange,
                        PRBool *aUsesIndependentSelection);
  nsresult FindItNow(nsIPresShell *aPresShell, PRBool aIsLinksOnly,
                     PRBool aIsFirstVisiblePreferred, PRBool aFindPrev,
                     PRUint16* aResult);
  nsresult GetSearchContainers(nsISupports *aContainer,
                               nsISelectionController *aSelectionController,
                               PRBool aIsFirstVisiblePreferred,
                               PRBool aFindPrev, nsIPresShell **aPresShell,
                               nsPresContext **aPresContext);

  // Get the pres shell from mPresShell and return it only if it is still
  // attached to the DOM window.
  NS_HIDDEN_(already_AddRefed<nsIPresShell>) GetPresShell();

  // Current find state
  nsString mTypeAheadBuffer;
  nsCString mNotFoundSoundURL;

  // PRBool's are used instead of PRPackedBool's where the address of the
  // boolean variable is getting passed into a method. For example:
  // GetBoolPref("accessibility.typeaheadfind.linksonly", &mLinksOnlyPref);
  PRBool mLinksOnlyPref;
  PRBool mStartLinksOnlyPref;
  PRPackedBool mLinksOnly;
  PRBool mCaretBrowsingOn;
  nsCOMPtr<nsIDOMElement> mFoundLink;     // Most recent elem found, if a link
  nsCOMPtr<nsIDOMElement> mFoundEditable; // Most recent elem found, if editable
  nsCOMPtr<nsIDOMWindow> mCurrentWindow;
  // mLastFindLength is the character length of the last find string.  It is used for
  // disabling the "not found" sound when using backspace or delete
  PRUint32 mLastFindLength;

  // Sound is played asynchronously on some platforms.
  // If we destroy mSoundInterface before sound has played, it won't play
  nsCOMPtr<nsISound> mSoundInterface;
  PRBool mIsSoundInitialized;
  
  // where selection was when user started the find
  nsCOMPtr<nsIDOMRange> mStartFindRange;
  nsCOMPtr<nsIDOMRange> mSearchRange;
  nsCOMPtr<nsIDOMRange> mStartPointRange;
  nsCOMPtr<nsIDOMRange> mEndPointRange;

  // Cached useful interfaces
  nsCOMPtr<nsIFind> mFind;
  nsCOMPtr<nsIWebBrowserFind> mWebBrowserFind;

  // The focused content window that we're listening to and its cached objects
  nsWeakPtr mDocShell;
  nsWeakPtr mPresShell;
  nsWeakPtr mSelectionController;
                                          // Most recent match's controller
};
