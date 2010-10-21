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
 *   Asaf Romano     (mano@mozilla.com)
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

#include "nsCOMPtr.h"
#include "nsMemory.h"
#include "nsIServiceManager.h"
#include "mozilla/ModuleUtils.h"
#include "nsIWebBrowserChrome.h"
#include "nsCURILoader.h"
#include "nsNetUtil.h"
#include "nsIURL.h"
#include "nsIURI.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIEditorDocShell.h"
#include "nsISimpleEnumerator.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMNSEvent.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranch2.h"
#include "nsIPrefService.h"
#include "nsString.h"
#include "nsCRT.h"

#include "nsIDOMNode.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsFrameTraversal.h"
#include "nsIDOMDocument.h"
#include "nsIImageDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMNSHTMLDocument.h"
#include "nsIDOMHTMLElement.h"
#include "nsIEventStateManager.h"
#include "nsIDocument.h"
#include "nsISelection.h"
#include "nsISelectElement.h"
#include "nsILink.h"
#include "nsTextFragment.h"
#include "nsIDOMNSEditableElement.h"
#include "nsIDOMNSHTMLElement.h"
#include "nsIEditor.h"

#include "nsIDocShellTreeItem.h"
#include "nsIWebNavigation.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsContentCID.h"
#include "nsLayoutCID.h"
#include "nsWidgetsCID.h"
#include "nsIFormControl.h"
#include "nsINameSpaceManager.h"
#include "nsIWindowWatcher.h"
#include "nsIObserverService.h"
#include "nsFocusManager.h"

#include "nsTypeAheadFind.h"

NS_INTERFACE_MAP_BEGIN(nsTypeAheadFind)
  NS_INTERFACE_MAP_ENTRY(nsITypeAheadFind)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsITypeAheadFind)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsTypeAheadFind)
NS_IMPL_RELEASE(nsTypeAheadFind)

static NS_DEFINE_IID(kRangeCID, NS_RANGE_CID);
static NS_DEFINE_CID(kFrameTraversalCID, NS_FRAMETRAVERSAL_CID);

#define NS_FIND_CONTRACTID "@mozilla.org/embedcomp/rangefind;1"

nsTypeAheadFind::nsTypeAheadFind():
  mStartLinksOnlyPref(PR_FALSE),
  mCaretBrowsingOn(PR_FALSE),
  mLastFindLength(0),
  mIsSoundInitialized(PR_FALSE)
{
}

nsTypeAheadFind::~nsTypeAheadFind()
{
  nsCOMPtr<nsIPrefBranch2> prefInternal(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefInternal) {
    prefInternal->RemoveObserver("accessibility.typeaheadfind", this);
    prefInternal->RemoveObserver("accessibility.browsewithcaret", this);
  }
}

nsresult
nsTypeAheadFind::Init(nsIDocShell* aDocShell)
{
  nsCOMPtr<nsIPrefBranch2> prefInternal(do_GetService(NS_PREFSERVICE_CONTRACTID));
  mSearchRange = do_CreateInstance(kRangeCID);
  mStartPointRange = do_CreateInstance(kRangeCID);
  mEndPointRange = do_CreateInstance(kRangeCID);
  mFind = do_CreateInstance(NS_FIND_CONTRACTID);
  if (!prefInternal || !mSearchRange || !mStartPointRange || !mEndPointRange || !mFind)
    return NS_ERROR_FAILURE;

  SetDocShell(aDocShell);

  // ----------- Listen to prefs ------------------
  nsresult rv = prefInternal->AddObserver("accessibility.browsewithcaret", this, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  // ----------- Get initial preferences ----------
  PrefsReset();

  // ----------- Set search options ---------------
  mFind->SetCaseSensitive(PR_FALSE);
  mFind->SetWordBreaker(nsnull);

  return rv;
}

nsresult
nsTypeAheadFind::PrefsReset()
{
  nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
  NS_ENSURE_TRUE(prefBranch, NS_ERROR_FAILURE);

  prefBranch->GetBoolPref("accessibility.typeaheadfind.startlinksonly",
                          &mStartLinksOnlyPref);

  PRBool isSoundEnabled = PR_TRUE;
  prefBranch->GetBoolPref("accessibility.typeaheadfind.enablesound",
                           &isSoundEnabled);
  nsXPIDLCString soundStr;
  if (isSoundEnabled)
    prefBranch->GetCharPref("accessibility.typeaheadfind.soundURL", getter_Copies(soundStr));

  mNotFoundSoundURL = soundStr;

  prefBranch->GetBoolPref("accessibility.browsewithcaret",
                          &mCaretBrowsingOn);

  return NS_OK;
}

NS_IMETHODIMP
nsTypeAheadFind::SetCaseSensitive(PRBool isCaseSensitive)
{
  mFind->SetCaseSensitive(isCaseSensitive);
  return NS_OK;
}

NS_IMETHODIMP
nsTypeAheadFind::GetCaseSensitive(PRBool* isCaseSensitive)
{
  mFind->GetCaseSensitive(isCaseSensitive);
  return NS_OK;
}

NS_IMETHODIMP
nsTypeAheadFind::SetDocShell(nsIDocShell* aDocShell)
{
  mDocShell = do_GetWeakReference(aDocShell);

  mWebBrowserFind = do_GetInterface(aDocShell);
  NS_ENSURE_TRUE(mWebBrowserFind, NS_ERROR_FAILURE);

  nsCOMPtr<nsIPresShell> presShell;
  aDocShell->GetPresShell(getter_AddRefs(presShell));
  mPresShell = do_GetWeakReference(presShell);      

  mStartFindRange = nsnull;
  mStartPointRange = do_CreateInstance(kRangeCID);
  mSearchRange = do_CreateInstance(kRangeCID);

  mFoundLink = nsnull;
  mFoundEditable = nsnull;
  mCurrentWindow = nsnull;

  mSelectionController = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsTypeAheadFind::SetSelectionModeAndRepaint(PRInt16 aToggle)
{
  nsCOMPtr<nsISelectionController> selectionController = 
    do_QueryReferent(mSelectionController);
  if (!selectionController) {
    return NS_OK;
  }

  selectionController->SetDisplaySelection(aToggle);
  selectionController->RepaintSelection(nsISelectionController::SELECTION_NORMAL);

  return NS_OK;
}

NS_IMETHODIMP
nsTypeAheadFind::CollapseSelection()
{
  nsCOMPtr<nsISelectionController> selectionController = 
    do_QueryReferent(mSelectionController);
  if (!selectionController) {
    return NS_OK;
  }

  nsCOMPtr<nsISelection> selection;
  selectionController->GetSelection(nsISelectionController::SELECTION_NORMAL,
                                     getter_AddRefs(selection));
  if (selection)
    selection->CollapseToStart();

  return NS_OK;
}

NS_IMETHODIMP
nsTypeAheadFind::Observe(nsISupports *aSubject, const char *aTopic,
                         const PRUnichar *aData)
{
  if (!nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID))
    return PrefsReset();

  return NS_OK;
}

void
nsTypeAheadFind::SaveFind()
{
  if (mWebBrowserFind)
    mWebBrowserFind->SetSearchString(mTypeAheadBuffer.get());
  
  // save the length of this find for "not found" sound
  mLastFindLength = mTypeAheadBuffer.Length();
}

void
nsTypeAheadFind::PlayNotFoundSound()
{
  if (mNotFoundSoundURL.IsEmpty())    // no sound
    return;

  if (!mSoundInterface)
    mSoundInterface = do_CreateInstance("@mozilla.org/sound;1");

  if (mSoundInterface) {
    mIsSoundInitialized = PR_TRUE;

    if (mNotFoundSoundURL.Equals("beep")) {
      mSoundInterface->Beep();
      return;
    }

    nsCOMPtr<nsIURI> soundURI;
    if (mNotFoundSoundURL.Equals("default"))
      NS_NewURI(getter_AddRefs(soundURI), NS_LITERAL_CSTRING(TYPEAHEADFIND_NOTFOUND_WAV_URL));
    else
      NS_NewURI(getter_AddRefs(soundURI), mNotFoundSoundURL);

    nsCOMPtr<nsIURL> soundURL(do_QueryInterface(soundURI));
    if (soundURL)
      mSoundInterface->Play(soundURL);
  }
}

nsresult
nsTypeAheadFind::FindItNow(nsIPresShell *aPresShell, PRBool aIsLinksOnly,
                           PRBool aIsFirstVisiblePreferred, PRBool aFindPrev,
                           PRUint16* aResult)
{
  *aResult = FIND_NOTFOUND;
  mFoundLink = nsnull;
  mFoundEditable = nsnull;
  mCurrentWindow = nsnull;
  nsCOMPtr<nsIPresShell> startingPresShell (GetPresShell());
  if (!startingPresShell) {    
    nsCOMPtr<nsIDocShell> ds = do_QueryReferent(mDocShell);
    NS_ENSURE_TRUE(ds, NS_ERROR_FAILURE);

    ds->GetPresShell(getter_AddRefs(startingPresShell));
    mPresShell = do_GetWeakReference(startingPresShell);    
  }  

  nsCOMPtr<nsIPresShell> presShell(aPresShell);

  if (!presShell) {
    presShell = startingPresShell;  // this is the current document

    if (!presShell)
      return NS_ERROR_FAILURE;
  }

  nsRefPtr<nsPresContext> presContext = presShell->GetPresContext();

  if (!presContext)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsISelection> selection;
  nsCOMPtr<nsISelectionController> selectionController = 
    do_QueryReferent(mSelectionController);
  if (!selectionController) {
    GetSelection(presShell, getter_AddRefs(selectionController),
                 getter_AddRefs(selection)); // cache for reuse
    mSelectionController = do_GetWeakReference(selectionController);
  } else {
    selectionController->GetSelection(
      nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));
  }
 
  nsCOMPtr<nsISupports> startingContainer = presContext->GetContainer();
  nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(startingContainer));
  NS_ASSERTION(treeItem, "Bug 175321 Crashes with Type Ahead Find [@ nsTypeAheadFind::FindItNow]");
  if (!treeItem)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocShellTreeItem> rootContentTreeItem;
  nsCOMPtr<nsIDocShell> currentDocShell;
  nsCOMPtr<nsIDocShell> startingDocShell(do_QueryInterface(startingContainer));

  treeItem->GetSameTypeRootTreeItem(getter_AddRefs(rootContentTreeItem));
  nsCOMPtr<nsIDocShell> rootContentDocShell =
    do_QueryInterface(rootContentTreeItem);

  if (!rootContentDocShell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsISimpleEnumerator> docShellEnumerator;
  rootContentDocShell->GetDocShellEnumerator(nsIDocShellTreeItem::typeContent,
                                             nsIDocShell::ENUMERATE_FORWARDS,
                                             getter_AddRefs(docShellEnumerator));

  // Default: can start at the current document
  nsCOMPtr<nsISupports> currentContainer = startingContainer =
    do_QueryInterface(rootContentDocShell);

  // Iterate up to current shell, if there's more than 1 that we're
  // dealing with
  PRBool hasMoreDocShells;

  while (NS_SUCCEEDED(docShellEnumerator->HasMoreElements(&hasMoreDocShells)) && hasMoreDocShells) {
    docShellEnumerator->GetNext(getter_AddRefs(currentContainer));
    currentDocShell = do_QueryInterface(currentContainer);
    if (!currentDocShell || currentDocShell == startingDocShell || aIsFirstVisiblePreferred)
      break;    
  }

  // ------------ Get ranges ready ----------------
  nsCOMPtr<nsIDOMRange> returnRange;
  nsCOMPtr<nsIPresShell> focusedPS;
  if (NS_FAILED(GetSearchContainers(currentContainer,
                                    (!aIsFirstVisiblePreferred ||
                                     mStartFindRange) ?
                                    selectionController.get() : nsnull,
                                    aIsFirstVisiblePreferred,  aFindPrev,
                                    getter_AddRefs(presShell),
                                    getter_AddRefs(presContext)))) {
    return NS_ERROR_FAILURE;
  }

  PRInt16 rangeCompareResult = 0;
  mStartPointRange->CompareBoundaryPoints(nsIDOMRange::START_TO_START, mSearchRange, &rangeCompareResult);
  // No need to wrap find in doc if starting at beginning
  PRBool hasWrapped = (rangeCompareResult < 0);

  if (mTypeAheadBuffer.IsEmpty())
    return NS_ERROR_FAILURE;

  mFind->SetFindBackwards(aFindPrev);

  while (PR_TRUE) {    // ----- Outer while loop: go through all docs -----
    while (PR_TRUE) {  // === Inner while loop: go through a single doc ===
      mFind->Find(mTypeAheadBuffer.get(), mSearchRange, mStartPointRange,
                  mEndPointRange, getter_AddRefs(returnRange));
      
      if (!returnRange)
        break;  // Nothing found in this doc, go to outer loop (try next doc)

      // ------- Test resulting found range for success conditions ------
      PRBool isInsideLink = PR_FALSE, isStartingLink = PR_FALSE;

      if (aIsLinksOnly) {
        // Don't check if inside link when searching all text
        RangeStartsInsideLink(returnRange, presShell, &isInsideLink,
                              &isStartingLink);
      }

      PRBool usesIndependentSelection;
      if (!IsRangeVisible(presShell, presContext, returnRange,
                          aIsFirstVisiblePreferred, PR_FALSE,
                          getter_AddRefs(mStartPointRange), 
                          &usesIndependentSelection) ||
          (aIsLinksOnly && !isInsideLink) ||
          (mStartLinksOnlyPref && aIsLinksOnly && !isStartingLink)) {
        // ------ Failure ------
        // Start find again from here
        returnRange->CloneRange(getter_AddRefs(mStartPointRange));

        // Collapse to end
        mStartPointRange->Collapse(aFindPrev);

        continue;
      }

      // ------ Success! -------
      // Hide old selection (new one may be on a different controller)
      if (selection) {
        selection->CollapseToStart();
        SetSelectionModeAndRepaint(nsISelectionController::SELECTION_ON);
      }

      // Make sure new document is selected
      if (presShell != startingPresShell) {
        // We are in a new document (because of frames/iframes)
        mPresShell = do_GetWeakReference(presShell);
      }

      nsCOMPtr<nsIDocument> document =
        do_QueryInterface(presShell->GetDocument());
      NS_ASSERTION(document, "Wow, presShell doesn't have document!");
      if (!document)
        return NS_ERROR_UNEXPECTED;

      nsCOMPtr<nsPIDOMWindow> window = document->GetWindow();
      NS_ASSERTION(window, "document has no window");
      if (!window)
        return NS_ERROR_UNEXPECTED;

      nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
      if (usesIndependentSelection) {
        /* If a search result is found inside an editable element, we'll focus
         * the element only if focus is in our content window, i.e.
         * |if (focusedWindow.top == ourWindow.top)| */
        PRBool shouldFocusEditableElement = false;
        if (fm) {
          nsCOMPtr<nsIDOMWindow> focusedWindow;
          nsresult rv = fm->GetFocusedWindow(getter_AddRefs(focusedWindow));
          if (NS_SUCCEEDED(rv)) {
            nsCOMPtr<nsPIDOMWindow> fwPI(do_QueryInterface(focusedWindow, &rv));
            if (NS_SUCCEEDED(rv)) {
              nsCOMPtr<nsIDocShellTreeItem> fwTreeItem
                (do_QueryInterface(fwPI->GetDocShell(), &rv));
              if (NS_SUCCEEDED(rv)) {
                nsCOMPtr<nsIDocShellTreeItem> fwRootTreeItem;
                rv = fwTreeItem->GetSameTypeRootTreeItem(getter_AddRefs(fwRootTreeItem));
                if (NS_SUCCEEDED(rv) && fwRootTreeItem == rootContentTreeItem)
                  shouldFocusEditableElement = PR_TRUE;
              }
            }
          }
        }

        // We may be inside an editable element, and therefore the selection
        // may be controlled by a different selection controller.  Walk up the
        // chain of parent nodes to see if we find one.
        nsCOMPtr<nsIDOMNode> node;
        returnRange->GetStartContainer(getter_AddRefs(node));
        while (node) {
          nsCOMPtr<nsIDOMNSEditableElement> editable = do_QueryInterface(node);
          if (editable) {
            // Inside an editable element.  Get the correct selection 
            // controller and selection.
            nsCOMPtr<nsIEditor> editor;
            editable->GetEditor(getter_AddRefs(editor));
            NS_ASSERTION(editor, "Editable element has no editor!");
            if (!editor) {
              break;
            }
            editor->GetSelectionController(
              getter_AddRefs(selectionController));
            if (selectionController) {
              selectionController->GetSelection(
                nsISelectionController::SELECTION_NORMAL, 
                getter_AddRefs(selection));
            }
            mFoundEditable = do_QueryInterface(node);

            if (!shouldFocusEditableElement)
              break;

            // Otherwise move focus/caret to editable element
            if (fm)
              fm->SetFocus(mFoundEditable, 0);
            break;
          }
          nsIDOMNode* tmp = node;
          tmp->GetParentNode(getter_AddRefs(node));
        }

        // If we reach here without setting mFoundEditable, then something
        // besides editable elements can cause us to have an independent
        // selection controller.  I don't know whether this is possible.
        // Currently, we simply fall back to grabbing the document's selection
        // controller in this case.  Perhaps we should reject this find match
        // and search again.
        NS_ASSERTION(mFoundEditable, "Independent selection controller on "
                     "non-editable element!");
      }

      if (!mFoundEditable) {
        // Not using a separate selection controller, so just get the
        // document's controller and selection.
        GetSelection(presShell, getter_AddRefs(selectionController), 
                     getter_AddRefs(selection));
      }
      mSelectionController = do_GetWeakReference(selectionController);

      // Select the found text
      if (selection) {
        selection->RemoveAllRanges();
        selection->AddRange(returnRange);
      }

      if (!mFoundEditable && fm) {
        nsCOMPtr<nsIDOMWindow> win = do_QueryInterface(window);
        fm->MoveFocus(win, nsnull, nsIFocusManager::MOVEFOCUS_CARET,
                      nsIFocusManager::FLAG_NOSCROLL | nsIFocusManager::FLAG_NOSWITCHFRAME,
                      getter_AddRefs(mFoundLink));
      }

      // Change selection color to ATTENTION and scroll to it.  Careful: we
      // must wait until after we goof with focus above before changing to
      // ATTENTION, or when we MoveFocus() and the selection is not on a
      // link, we'll blur, which will lose the ATTENTION.
      if (selectionController) {
        // Beware! This may flush notifications via synchronous
        // ScrollSelectionIntoView.
        SetSelectionModeAndRepaint(nsISelectionController::SELECTION_ATTENTION);
        selectionController->ScrollSelectionIntoView(
          nsISelectionController::SELECTION_NORMAL, 
          nsISelectionController::SELECTION_FOCUS_REGION,
          nsISelectionController::SCROLL_SYNCHRONOUS);
      }

      mCurrentWindow = window;
      *aResult = hasWrapped ? FIND_WRAPPED : FIND_FOUND;
      return NS_OK;
    }

    // ======= end-inner-while (go through a single document) ==========

    // ---------- Nothing found yet, try next document  -------------
    PRBool hasTriedFirstDoc = PR_FALSE;
    do {
      // ==== Second inner loop - get another while  ====
      if (NS_SUCCEEDED(docShellEnumerator->HasMoreElements(&hasMoreDocShells))
          && hasMoreDocShells) {
        docShellEnumerator->GetNext(getter_AddRefs(currentContainer));
        NS_ASSERTION(currentContainer, "HasMoreElements lied to us!");
        currentDocShell = do_QueryInterface(currentContainer);

        if (currentDocShell)
          break;
      }
      else if (hasTriedFirstDoc)  // Avoid potential infinite loop
        return NS_ERROR_FAILURE;  // No content doc shells

      // Reached last doc shell, loop around back to first doc shell
      rootContentDocShell->GetDocShellEnumerator(nsIDocShellTreeItem::typeContent,
                                                 nsIDocShell::ENUMERATE_FORWARDS,
                                                 getter_AddRefs(docShellEnumerator));
      hasTriedFirstDoc = PR_TRUE;      
    } while (docShellEnumerator);  // ==== end second inner while  ===

    PRBool continueLoop = PR_FALSE;
    if (currentDocShell != startingDocShell)
      continueLoop = PR_TRUE;  // Try next document
    else if (!hasWrapped || aIsFirstVisiblePreferred) {
      // Finished searching through docshells:
      // If aFirstVisiblePreferred == PR_TRUE, we may need to go through all
      // docshells twice -once to look for visible matches, the second time
      // for any match
      aIsFirstVisiblePreferred = PR_FALSE;
      hasWrapped = PR_TRUE;
      continueLoop = PR_TRUE; // Go through all docs again
    }

    if (continueLoop) {
      if (NS_FAILED(GetSearchContainers(currentContainer, nsnull,
                                        aIsFirstVisiblePreferred, aFindPrev,
                                        getter_AddRefs(presShell),
                                        getter_AddRefs(presContext)))) {
        continue;
      }

      if (aFindPrev) {
        // Reverse mode: swap start and end points, so that we start
        // at end of document and go to beginning
        nsCOMPtr<nsIDOMRange> tempRange;
        mStartPointRange->CloneRange(getter_AddRefs(tempRange));
        mStartPointRange = mEndPointRange;
        mEndPointRange = tempRange;
      }

      continue;
    }

    // ------------- Failed --------------
    break;
  }   // end-outer-while: go through all docs

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsTypeAheadFind::GetSearchString(nsAString& aSearchString)
{
  aSearchString = mTypeAheadBuffer;
  return NS_OK;
}

NS_IMETHODIMP
nsTypeAheadFind::GetFoundLink(nsIDOMElement** aFoundLink)
{
  NS_ENSURE_ARG_POINTER(aFoundLink);
  *aFoundLink = mFoundLink;
  NS_IF_ADDREF(*aFoundLink);
  return NS_OK;
}

NS_IMETHODIMP
nsTypeAheadFind::GetFoundEditable(nsIDOMElement** aFoundEditable)
{
  NS_ENSURE_ARG_POINTER(aFoundEditable);
  *aFoundEditable = mFoundEditable;
  NS_IF_ADDREF(*aFoundEditable);
  return NS_OK;
}

NS_IMETHODIMP
nsTypeAheadFind::GetCurrentWindow(nsIDOMWindow** aCurrentWindow)
{
  NS_ENSURE_ARG_POINTER(aCurrentWindow);
  *aCurrentWindow = mCurrentWindow;
  NS_IF_ADDREF(*aCurrentWindow);
  return NS_OK;
}

nsresult
nsTypeAheadFind::GetSearchContainers(nsISupports *aContainer,
                                     nsISelectionController *aSelectionController,
                                     PRBool aIsFirstVisiblePreferred,
                                     PRBool aFindPrev,
                                     nsIPresShell **aPresShell,
                                     nsPresContext **aPresContext)
{
  NS_ENSURE_ARG_POINTER(aContainer);
  NS_ENSURE_ARG_POINTER(aPresShell);
  NS_ENSURE_ARG_POINTER(aPresContext);

  *aPresShell = nsnull;
  *aPresContext = nsnull;

  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aContainer));
  if (!docShell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPresShell> presShell;
  docShell->GetPresShell(getter_AddRefs(presShell));

  nsRefPtr<nsPresContext> presContext;
  docShell->GetPresContext(getter_AddRefs(presContext));

  if (!presShell || !presContext)
    return NS_ERROR_FAILURE;

  nsIDocument* doc = presShell->GetDocument();

  if (!doc)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> rootContent;
  nsCOMPtr<nsIDOMHTMLDocument> htmlDoc(do_QueryInterface(doc));
  if (htmlDoc) {
    nsCOMPtr<nsIDOMHTMLElement> bodyEl;
    htmlDoc->GetBody(getter_AddRefs(bodyEl));
    rootContent = do_QueryInterface(bodyEl);
  }

  if (!rootContent)
    rootContent = doc->GetRootElement();
 
  nsCOMPtr<nsIDOMNode> rootNode(do_QueryInterface(rootContent));

  if (!rootNode)
    return NS_ERROR_FAILURE;

  PRUint32 childCount = rootContent->GetChildCount();

  mSearchRange->SelectNodeContents(rootNode);

  mEndPointRange->SetEnd(rootNode, childCount);
  mEndPointRange->Collapse(PR_FALSE); // collapse to end

  // Consider current selection as null if
  // it's not in the currently focused document
  nsCOMPtr<nsIDOMRange> currentSelectionRange;
  nsCOMPtr<nsIPresShell> selectionPresShell = GetPresShell();
  if (aSelectionController && selectionPresShell && selectionPresShell == presShell) {
    nsCOMPtr<nsISelection> selection;
    aSelectionController->GetSelection(
      nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));
    if (selection)
      selection->GetRangeAt(0, getter_AddRefs(currentSelectionRange));
  }

  if (!currentSelectionRange) {
    // Ensure visible range, move forward if necessary
    // This uses ignores the return value, but usese the side effect of
    // IsRangeVisible. It returns the first visible range after searchRange
    IsRangeVisible(presShell, presContext, mSearchRange, 
                   aIsFirstVisiblePreferred, PR_TRUE, 
                   getter_AddRefs(mStartPointRange), nsnull);
  }
  else {
    PRInt32 startOffset;
    nsCOMPtr<nsIDOMNode> startNode;
    if (aFindPrev) {
      currentSelectionRange->GetStartContainer(getter_AddRefs(startNode));
      currentSelectionRange->GetStartOffset(&startOffset);
    } else {
      currentSelectionRange->GetEndContainer(getter_AddRefs(startNode));
      currentSelectionRange->GetEndOffset(&startOffset);
    }
    if (!startNode)
      startNode = rootNode;    

    // We need to set the start point this way, other methods haven't worked
    mStartPointRange->SelectNode(startNode);
    mStartPointRange->SetStart(startNode, startOffset);
  }

  mStartPointRange->Collapse(PR_TRUE); // collapse to start

  *aPresShell = presShell;
  NS_ADDREF(*aPresShell);

  *aPresContext = presContext;
  NS_ADDREF(*aPresContext);

  return NS_OK;
}

void
nsTypeAheadFind::RangeStartsInsideLink(nsIDOMRange *aRange,
                                       nsIPresShell *aPresShell,
                                       PRBool *aIsInsideLink,
                                       PRBool *aIsStartingLink)
{
  *aIsInsideLink = PR_FALSE;
  *aIsStartingLink = PR_TRUE;

  // ------- Get nsIContent to test -------
  nsCOMPtr<nsIDOMNode> startNode;
  nsCOMPtr<nsIContent> startContent, origContent;
  aRange->GetStartContainer(getter_AddRefs(startNode));
  PRInt32 startOffset;
  aRange->GetStartOffset(&startOffset);

  startContent = do_QueryInterface(startNode);
  if (!startContent) {
    NS_NOTREACHED("startContent should never be null");
    return;
  }
  origContent = startContent;

  if (startContent->IsElement()) {
    nsIContent *childContent = startContent->GetChildAt(startOffset);
    if (childContent) {
      startContent = childContent;
    }
  }
  else if (startOffset > 0) {
    const nsTextFragment *textFrag = startContent->GetText();
    if (textFrag) {
      // look for non whitespace character before start offset
      for (PRInt32 index = 0; index < startOffset; index++) {
        if (!XP_IS_SPACE(textFrag->CharAt(index))) {
          *aIsStartingLink = PR_FALSE;  // not at start of a node

          break;
        }
      }
    }
  }

  // ------- Check to see if inside link ---------

  // We now have the correct start node for the range
  // Search for links, starting with startNode, and going up parent chain

  nsCOMPtr<nsIAtom> tag, hrefAtom(do_GetAtom("href"));
  nsCOMPtr<nsIAtom> typeAtom(do_GetAtom("type"));

  while (PR_TRUE) {
    // Keep testing while startContent is equal to something,
    // eventually we'll run out of ancestors

    if (startContent->IsHTML()) {
      nsCOMPtr<nsILink> link(do_QueryInterface(startContent));
      if (link) {
        // Check to see if inside HTML link
        *aIsInsideLink = startContent->HasAttr(kNameSpaceID_None, hrefAtom);
        return;
      }
    }
    else {
      // Any xml element can be an xlink
      *aIsInsideLink = startContent->HasAttr(kNameSpaceID_XLink, hrefAtom);
      if (*aIsInsideLink) {
        if (!startContent->AttrValueIs(kNameSpaceID_XLink, typeAtom,
                                       NS_LITERAL_STRING("simple"),
                                       eCaseMatters)) {
          *aIsInsideLink = PR_FALSE;  // Xlink must be type="simple"
        }

        return;
      }
    }

    // Get the parent
    nsCOMPtr<nsIContent> parent = startContent->GetParent();
    if (!parent)
      break;

    nsIContent *parentsFirstChild = parent->GetChildAt(0);

    // We don't want to look at a whitespace-only first child
    if (parentsFirstChild && parentsFirstChild->TextIsOnlyWhitespace()) {
      parentsFirstChild = parent->GetChildAt(1);
    }

    if (parentsFirstChild != startContent) {
      // startContent wasn't a first child, so we conclude that
      // if this is inside a link, it's not at the beginning of it
      *aIsStartingLink = PR_FALSE;
    }

    startContent = parent;
  }

  *aIsStartingLink = PR_FALSE;
}

/* Find another match in the page. */
NS_IMETHODIMP
nsTypeAheadFind::FindAgain(PRBool aFindBackwards, PRBool aLinksOnly,
                           PRUint16* aResult)

{
  *aResult = FIND_NOTFOUND;

  if (!mTypeAheadBuffer.IsEmpty())
    // Beware! This may flush notifications via synchronous
    // ScrollSelectionIntoView.
    FindItNow(nsnull, aLinksOnly, PR_FALSE, aFindBackwards, aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsTypeAheadFind::Find(const nsAString& aSearchString, PRBool aLinksOnly,
                      PRUint16* aResult)
{
  *aResult = FIND_NOTFOUND;

  nsCOMPtr<nsIPresShell> presShell (GetPresShell());
  if (!presShell) {    
    nsCOMPtr<nsIDocShell> ds (do_QueryReferent(mDocShell));
    NS_ENSURE_TRUE(ds, NS_ERROR_FAILURE);

    ds->GetPresShell(getter_AddRefs(presShell));
    mPresShell = do_GetWeakReference(presShell);    
  }  
  nsCOMPtr<nsISelection> selection;
  nsCOMPtr<nsISelectionController> selectionController = 
    do_QueryReferent(mSelectionController);
  if (!selectionController) {
    GetSelection(presShell, getter_AddRefs(selectionController),
                 getter_AddRefs(selection)); // cache for reuse
    mSelectionController = do_GetWeakReference(selectionController);
  } else {
    selectionController->GetSelection(
      nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));
  }

  if (selection)
    selection->CollapseToStart();

  if (aSearchString.IsEmpty()) {
    mTypeAheadBuffer.Truncate();

    // These will be initialized to their true values after the first character
    // is typed
    mStartFindRange = nsnull;
    mSelectionController = nsnull;

    *aResult = FIND_FOUND;
    return NS_OK;
  }

  PRBool atEnd = PR_FALSE;    
  if (mTypeAheadBuffer.Length()) {
    const nsAString& oldStr = Substring(mTypeAheadBuffer, 0, mTypeAheadBuffer.Length());
    const nsAString& newStr = Substring(aSearchString, 0, mTypeAheadBuffer.Length());
    if (oldStr.Equals(newStr))
      atEnd = PR_TRUE;
  
    const nsAString& newStr2 = Substring(aSearchString, 0, aSearchString.Length());
    const nsAString& oldStr2 = Substring(mTypeAheadBuffer, 0, aSearchString.Length());
    if (oldStr2.Equals(newStr2))
      atEnd = PR_TRUE;
    
    if (!atEnd)
      mStartFindRange = nsnull;
  }

  if (!mIsSoundInitialized && !mNotFoundSoundURL.IsEmpty()) {
    // This makes sure system sound library is loaded so that
    // there's no lag before the first sound is played
    // by waiting for the first keystroke, we still get the startup time benefits.
    mIsSoundInitialized = PR_TRUE;
    mSoundInterface = do_CreateInstance("@mozilla.org/sound;1");
    if (mSoundInterface && !mNotFoundSoundURL.Equals(NS_LITERAL_CSTRING("beep"))) {
      mSoundInterface->Init();
    }
  }

#ifdef XP_WIN
  // After each keystroke, ensure sound object is destroyed, to free up memory 
  // allocated for error sound, otherwise Windows' nsISound impl 
  // holds onto the last played sound, using up memory.
  mSoundInterface = nsnull;
#endif

  PRInt32 bufferLength = mTypeAheadBuffer.Length();

  mTypeAheadBuffer = aSearchString;

  PRBool isFirstVisiblePreferred = PR_FALSE;

  // --------- Initialize find if 1st char ----------
  if (bufferLength == 0) {
    // If you can see the selection (not collapsed or thru caret browsing),
    // or if already focused on a page element, start there.
    // Otherwise we're going to start at the first visible element
    PRBool isSelectionCollapsed = PR_TRUE;
    if (selection)
      selection->GetIsCollapsed(&isSelectionCollapsed);

    // If true, we will scan from top left of visible area
    // If false, we will scan from start of selection
    isFirstVisiblePreferred = !atEnd && !mCaretBrowsingOn && isSelectionCollapsed;
    if (isFirstVisiblePreferred) {
      // Get the focused content. If there is a focused node, ensure the
      // selection is at that point. Otherwise, we will just want to start
      // from the caret position or the beginning of the document.
      nsPresContext* presContext = presShell->GetPresContext();
      NS_ENSURE_TRUE(presContext, NS_OK);

      nsCOMPtr<nsIDocument> document =
        do_QueryInterface(presShell->GetDocument());
      if (!document)
        return NS_ERROR_UNEXPECTED;

      nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(document->GetWindow());

      nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
      if (fm) {
        nsCOMPtr<nsIDOMElement> focusedElement;
        nsCOMPtr<nsIDOMWindow> focusedWindow;
        fm->GetFocusedElementForWindow(window, PR_FALSE, getter_AddRefs(focusedWindow),
                                       getter_AddRefs(focusedElement));
        // If the root element is focused, then it's actually the document
        // that has the focus, so ignore this.
        if (focusedElement &&
            !SameCOMIdentity(focusedElement, document->GetRootElement())) {
          fm->MoveCaretToFocus(window);
          isFirstVisiblePreferred = PR_FALSE;
        }
      }
    }
  }

  // ----------- Find the text! ---------------------
  // Beware! This may flush notifications via synchronous
  // ScrollSelectionIntoView.
  nsresult rv = FindItNow(nsnull, aLinksOnly, isFirstVisiblePreferred,
                          PR_FALSE, aResult);

  // ---------Handle success or failure ---------------
  if (NS_SUCCEEDED(rv)) {
    if (mTypeAheadBuffer.Length() == 1) {
      // If first letter, store where the first find succeeded
      // (mStartFindRange)

      mStartFindRange = nsnull;
      if (selection) {
        nsCOMPtr<nsIDOMRange> startFindRange;
        selection->GetRangeAt(0, getter_AddRefs(startFindRange));
        if (startFindRange)
          startFindRange->CloneRange(getter_AddRefs(mStartFindRange));
      }
    }
  }
  else {
    // Error sound
    if (mTypeAheadBuffer.Length() > mLastFindLength)
      PlayNotFoundSound();
  }

  SaveFind();
  return NS_OK;
}

void
nsTypeAheadFind::GetSelection(nsIPresShell *aPresShell,
                              nsISelectionController **aSelCon,
                              nsISelection **aDOMSel)
{
  if (!aPresShell)
    return;

  // if aCurrentNode is nsnull, get selection for document
  *aDOMSel = nsnull;

  nsPresContext* presContext = aPresShell->GetPresContext();

  nsIFrame *frame = aPresShell->GetRootFrame();

  if (presContext && frame) {
    frame->GetSelectionController(presContext, aSelCon);
    if (*aSelCon) {
      (*aSelCon)->GetSelection(nsISelectionController::SELECTION_NORMAL,
                               aDOMSel);
    }
  }
}


PRBool
nsTypeAheadFind::IsRangeVisible(nsIPresShell *aPresShell,
                                nsPresContext *aPresContext,
                                nsIDOMRange *aRange, PRBool aMustBeInViewPort,
                                PRBool aGetTopVisibleLeaf,
                                nsIDOMRange **aFirstVisibleRange,
                                PRBool *aUsesIndependentSelection)
{
  NS_ASSERTION(aPresShell && aPresContext && aRange && aFirstVisibleRange, 
               "params are invalid");

  // We need to know if the range start is visible.
  // Otherwise, return a the first visible range start 
  // in aFirstVisibleRange

  aRange->CloneRange(aFirstVisibleRange);
  nsCOMPtr<nsIDOMNode> node;
  aRange->GetStartContainer(getter_AddRefs(node));

  nsCOMPtr<nsIContent> content(do_QueryInterface(node));
  if (!content)
    return PR_FALSE;

  nsIFrame *frame = content->GetPrimaryFrame();
  if (!frame)    
    return PR_FALSE;  // No frame! Not visible then.

  if (!frame->GetStyleVisibility()->IsVisible())
    return PR_FALSE;

  // Detect if we are _inside_ a text control, or something else with its own
  // selection controller.
  if (aUsesIndependentSelection) {
    *aUsesIndependentSelection = 
      (frame->GetStateBits() & NS_FRAME_INDEPENDENT_SELECTION);
  }

  // ---- We have a frame ----
  if (!aMustBeInViewPort)   
    return PR_TRUE; //  Don't need it to be on screen, just in rendering tree

  // Get the next in flow frame that contains the range start
  PRInt32 startRangeOffset, startFrameOffset, endFrameOffset;
  aRange->GetStartOffset(&startRangeOffset);
  while (PR_TRUE) {
    frame->GetOffsets(startFrameOffset, endFrameOffset);
    if (startRangeOffset < endFrameOffset)
      break;

    nsIFrame *nextContinuationFrame = frame->GetNextContinuation();
    if (nextContinuationFrame)
      frame = nextContinuationFrame;
    else
      break;
  }

  // Set up the variables we need, return true if we can't get at them all
  const PRUint16 kMinPixels  = 12;
  nscoord minDistance = nsPresContext::CSSPixelsToAppUnits(kMinPixels);

  // Get the bounds of the current frame, relative to the current view.
  // We don't use the more accurate AccGetBounds, because that is
  // more expensive and the STATE_OFFSCREEN flag that this is used
  // for only needs to be a rough indicator
  nsRectVisibility rectVisibility = nsRectVisibility_kAboveViewport;

  if (!aGetTopVisibleLeaf && !frame->GetRect().IsEmpty()) {
    rectVisibility =
      aPresShell->GetRectVisibility(frame,
                                    nsRect(nsPoint(0,0), frame->GetSize()),
                                    minDistance);

    if (rectVisibility != nsRectVisibility_kAboveViewport) {
      return PR_TRUE;
    }
  }

  // We know that the target range isn't usable because it's not in the
  // view port. Move range forward to first visible point,
  // this speeds us up a lot in long documents
  nsCOMPtr<nsIFrameEnumerator> frameTraversal;
  nsCOMPtr<nsIFrameTraversal> trav(do_CreateInstance(kFrameTraversalCID));
  if (trav)
    trav->NewFrameTraversal(getter_AddRefs(frameTraversal),
                            aPresContext, frame,
                            eLeaf,
                            PR_FALSE, // aVisual
                            PR_FALSE, // aLockInScrollView
                            PR_FALSE  // aFollowOOFs
                            );

  if (!frameTraversal)
    return PR_FALSE;

  while (rectVisibility == nsRectVisibility_kAboveViewport) {
    frameTraversal->Next();
    frame = frameTraversal->CurrentItem();
    if (!frame)
      return PR_FALSE;

    if (!frame->GetRect().IsEmpty()) {
      rectVisibility =
        aPresShell->GetRectVisibility(frame,
                                      nsRect(nsPoint(0,0), frame->GetSize()),
                                      minDistance);
    }
  }

  if (frame) {
    nsCOMPtr<nsIDOMNode> firstVisibleNode = do_QueryInterface(frame->GetContent());

    if (firstVisibleNode) {
      (*aFirstVisibleRange)->SelectNode(firstVisibleNode);
      frame->GetOffsets(startFrameOffset, endFrameOffset);
      (*aFirstVisibleRange)->SetStart(firstVisibleNode, startFrameOffset);
      (*aFirstVisibleRange)->Collapse(PR_TRUE);  // Collapse to start
    }
  }

  return PR_FALSE;
}

already_AddRefed<nsIPresShell>
nsTypeAheadFind::GetPresShell()
{
  if (!mPresShell)
    return nsnull;

  nsIPresShell *shell = nsnull;
  CallQueryReferent(mPresShell.get(), &shell);
  if (shell) {
    nsPresContext *pc = shell->GetPresContext();
    if (!pc || !nsCOMPtr<nsISupports>(pc->GetContainer())) {
      NS_RELEASE(shell);
    }
  }

  return shell;
}
