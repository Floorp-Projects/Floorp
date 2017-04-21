/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsMemory.h"
#include "nsIServiceManager.h"
#include "mozilla/ModuleUtils.h"
#include "mozilla/Services.h"
#include "nsIWebBrowserChrome.h"
#include "nsCURILoader.h"
#include "nsCycleCollectionParticipant.h"
#include "nsNetUtil.h"
#include "nsIURL.h"
#include "nsIURI.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeOwner.h"
#include "nsISimpleEnumerator.h"
#include "nsPIDOMWindow.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsString.h"
#include "nsCRT.h"

#include "nsIDOMNode.h"
#include "mozilla/dom/Element.h"
#include "nsIFrame.h"
#include "nsFrameTraversal.h"
#include "nsIImageDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDocument.h"
#include "nsISelection.h"
#include "nsTextFragment.h"
#include "nsIDOMNSEditableElement.h"
#include "nsIEditor.h"

#include "nsIDocShellTreeItem.h"
#include "nsIWebNavigation.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsContentCID.h"
#include "nsLayoutCID.h"
#include "nsWidgetsCID.h"
#include "nsIFormControl.h"
#include "nsNameSpaceManager.h"
#include "nsIWindowWatcher.h"
#include "nsIObserverService.h"
#include "nsFocusManager.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Link.h"
#include "nsRange.h"
#include "nsXBLBinding.h"

#include "nsTypeAheadFind.h"

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsTypeAheadFind)
  NS_INTERFACE_MAP_ENTRY(nsITypeAheadFind)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsITypeAheadFind)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsTypeAheadFind)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsTypeAheadFind)

NS_IMPL_CYCLE_COLLECTION(nsTypeAheadFind, mFoundLink, mFoundEditable,
                         mCurrentWindow, mStartFindRange, mSearchRange,
                         mStartPointRange, mEndPointRange, mSoundInterface,
                         mFind, mFoundRange)

static NS_DEFINE_CID(kFrameTraversalCID, NS_FRAMETRAVERSAL_CID);

#define NS_FIND_CONTRACTID "@mozilla.org/embedcomp/rangefind;1"

nsTypeAheadFind::nsTypeAheadFind():
  mStartLinksOnlyPref(false),
  mCaretBrowsingOn(false),
  mDidAddObservers(false),
  mLastFindLength(0),
  mIsSoundInitialized(false),
  mCaseSensitive(false),
  mEntireWord(false)
{
}

nsTypeAheadFind::~nsTypeAheadFind()
{
  nsCOMPtr<nsIPrefBranch> prefInternal(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefInternal) {
    prefInternal->RemoveObserver("accessibility.typeaheadfind", this);
    prefInternal->RemoveObserver("accessibility.browsewithcaret", this);
  }
}

nsresult
nsTypeAheadFind::Init(nsIDocShell* aDocShell)
{
  nsCOMPtr<nsIPrefBranch> prefInternal(do_GetService(NS_PREFSERVICE_CONTRACTID));

  mSearchRange = nullptr;
  mStartPointRange = nullptr;
  mEndPointRange = nullptr;
  if (!prefInternal || !EnsureFind())
    return NS_ERROR_FAILURE;

  SetDocShell(aDocShell);

  if (!mDidAddObservers) {
    mDidAddObservers = true;
    // ----------- Listen to prefs ------------------
    nsresult rv = prefInternal->AddObserver("accessibility.browsewithcaret", this, true);
    NS_ENSURE_SUCCESS(rv, rv);

    // ----------- Get initial preferences ----------
    PrefsReset();

    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->AddObserver(this, DOM_WINDOW_DESTROYED_TOPIC, true);
    }
  }
  return NS_OK;
}

nsresult
nsTypeAheadFind::PrefsReset()
{
  nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
  NS_ENSURE_TRUE(prefBranch, NS_ERROR_FAILURE);

  prefBranch->GetBoolPref("accessibility.typeaheadfind.startlinksonly",
                          &mStartLinksOnlyPref);

  bool isSoundEnabled = true;
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
nsTypeAheadFind::SetCaseSensitive(bool isCaseSensitive)
{
  mCaseSensitive = isCaseSensitive;

  if (mFind) {
    mFind->SetCaseSensitive(mCaseSensitive);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTypeAheadFind::GetCaseSensitive(bool* isCaseSensitive)
{
  *isCaseSensitive = mCaseSensitive;

  return NS_OK;
}

NS_IMETHODIMP
nsTypeAheadFind::SetEntireWord(bool isEntireWord)
{
  mEntireWord = isEntireWord;

  if (mFind) {
    mFind->SetEntireWord(mEntireWord);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTypeAheadFind::GetEntireWord(bool* isEntireWord)
{
  *isEntireWord = mEntireWord;

  return NS_OK;
}

NS_IMETHODIMP
nsTypeAheadFind::SetDocShell(nsIDocShell* aDocShell)
{
  mDocShell = do_GetWeakReference(aDocShell);

  mWebBrowserFind = do_GetInterface(aDocShell);
  NS_ENSURE_TRUE(mWebBrowserFind, NS_ERROR_FAILURE);

  nsCOMPtr<nsIPresShell> presShell;
  presShell = aDocShell->GetPresShell();
  mPresShell = do_GetWeakReference(presShell);

  ReleaseStrongMemberVariables();
  return NS_OK;
}

void
nsTypeAheadFind::ReleaseStrongMemberVariables()
{
  mStartFindRange = nullptr;
  mStartPointRange = nullptr;
  mSearchRange = nullptr;
  mEndPointRange = nullptr;

  mFoundLink = nullptr;
  mFoundEditable = nullptr;
  mFoundRange = nullptr;
  mCurrentWindow = nullptr;

  mSelectionController = nullptr;

  mFind = nullptr;
}

NS_IMETHODIMP
nsTypeAheadFind::SetSelectionModeAndRepaint(int16_t aToggle)
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
                         const char16_t *aData)
{
  if (!nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    return PrefsReset();
  }
  if (!nsCRT::strcmp(aTopic, DOM_WINDOW_DESTROYED_TOPIC) &&
             SameCOMIdentity(aSubject, mCurrentWindow)) {
    ReleaseStrongMemberVariables();
  }

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
    mIsSoundInitialized = true;

    if (mNotFoundSoundURL.EqualsLiteral("beep")) {
      mSoundInterface->Beep();
      return;
    }

    nsCOMPtr<nsIURI> soundURI;
    if (mNotFoundSoundURL.EqualsLiteral("default"))
      NS_NewURI(getter_AddRefs(soundURI), NS_LITERAL_CSTRING(TYPEAHEADFIND_NOTFOUND_WAV_URL));
    else
      NS_NewURI(getter_AddRefs(soundURI), mNotFoundSoundURL);

    nsCOMPtr<nsIURL> soundURL(do_QueryInterface(soundURI));
    if (soundURL)
      mSoundInterface->Play(soundURL);
  }
}

nsresult
nsTypeAheadFind::FindItNow(nsIPresShell *aPresShell, bool aIsLinksOnly,
                           bool aIsFirstVisiblePreferred, bool aFindPrev,
                           uint16_t* aResult)
{
  *aResult = FIND_NOTFOUND;
  mFoundLink = nullptr;
  mFoundEditable = nullptr;
  mFoundRange = nullptr;
  mCurrentWindow = nullptr;
  nsCOMPtr<nsIPresShell> startingPresShell (GetPresShell());
  if (!startingPresShell) {    
    nsCOMPtr<nsIDocShell> ds = do_QueryReferent(mDocShell);
    NS_ENSURE_TRUE(ds, NS_ERROR_FAILURE);

    startingPresShell = ds->GetPresShell();
    mPresShell = do_GetWeakReference(startingPresShell);    
  }  

  nsCOMPtr<nsIPresShell> presShell(aPresShell);

  if (!presShell) {
    presShell = startingPresShell;  // this is the current document

    if (!presShell)
      return NS_ERROR_FAILURE;
  }

  // There could be unflushed notifications which hide textareas or other
  // elements that we don't want to find text in.
  presShell->FlushPendingNotifications(mozilla::FlushType::Layout);

  RefPtr<nsPresContext> presContext = presShell->GetPresContext();

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
 
  nsCOMPtr<nsIDocShell> startingDocShell(presContext->GetDocShell());
  NS_ASSERTION(startingDocShell, "Bug 175321 Crashes with Type Ahead Find [@ nsTypeAheadFind::FindItNow]");
  if (!startingDocShell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocShellTreeItem> rootContentTreeItem;
  nsCOMPtr<nsIDocShell> currentDocShell;

  startingDocShell->GetSameTypeRootTreeItem(getter_AddRefs(rootContentTreeItem));
  nsCOMPtr<nsIDocShell> rootContentDocShell =
    do_QueryInterface(rootContentTreeItem);

  if (!rootContentDocShell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsISimpleEnumerator> docShellEnumerator;
  rootContentDocShell->GetDocShellEnumerator(nsIDocShellTreeItem::typeContent,
                                             nsIDocShell::ENUMERATE_FORWARDS,
                                             getter_AddRefs(docShellEnumerator));

  // Default: can start at the current document
  nsCOMPtr<nsISupports> currentContainer =
    do_QueryInterface(rootContentDocShell);

  // Iterate up to current shell, if there's more than 1 that we're
  // dealing with
  bool hasMoreDocShells;

  while (NS_SUCCEEDED(docShellEnumerator->HasMoreElements(&hasMoreDocShells)) && hasMoreDocShells) {
    docShellEnumerator->GetNext(getter_AddRefs(currentContainer));
    currentDocShell = do_QueryInterface(currentContainer);
    if (!currentDocShell || currentDocShell == startingDocShell || aIsFirstVisiblePreferred)
      break;    
  }

  // ------------ Get ranges ready ----------------
  nsCOMPtr<nsIDOMRange> returnRange;
  if (NS_FAILED(GetSearchContainers(currentContainer,
                                    (!aIsFirstVisiblePreferred ||
                                     mStartFindRange) ?
                                    selectionController.get() : nullptr,
                                    aIsFirstVisiblePreferred,  aFindPrev,
                                    getter_AddRefs(presShell),
                                    getter_AddRefs(presContext)))) {
    return NS_ERROR_FAILURE;
  }

  int16_t rangeCompareResult = 0;
  if (!mStartPointRange) {
    mStartPointRange = new nsRange(presShell->GetDocument());
  }

  mStartPointRange->CompareBoundaryPoints(nsIDOMRange::START_TO_START, mSearchRange, &rangeCompareResult);
  // No need to wrap find in doc if starting at beginning
  bool hasWrapped = (rangeCompareResult < 0);

  if (mTypeAheadBuffer.IsEmpty() || !EnsureFind())
    return NS_ERROR_FAILURE;

  mFind->SetFindBackwards(aFindPrev);

  while (true) {    // ----- Outer while loop: go through all docs -----
    while (true) {  // === Inner while loop: go through a single doc ===
      mFind->Find(mTypeAheadBuffer.get(), mSearchRange, mStartPointRange,
                  mEndPointRange, getter_AddRefs(returnRange));

      if (!returnRange)
        break;  // Nothing found in this doc, go to outer loop (try next doc)

      // ------- Test resulting found range for success conditions ------
      bool isInsideLink = false, isStartingLink = false;

      if (aIsLinksOnly) {
        // Don't check if inside link when searching all text
        RangeStartsInsideLink(returnRange, presShell, &isInsideLink,
                              &isStartingLink);
      }

      bool usesIndependentSelection;
      if (!IsRangeVisible(presShell, presContext, returnRange,
                          aIsFirstVisiblePreferred, false,
                          getter_AddRefs(mStartPointRange), 
                          &usesIndependentSelection) ||
          (aIsLinksOnly && !isInsideLink) ||
          (mStartLinksOnlyPref && aIsLinksOnly && !isStartingLink)) {
        // ------ Failure ------
        // At this point mStartPointRange got updated to the first
        // visible range in the viewport.  We _may_ be able to just
        // start there, if it's not taking us in the wrong direction.
        if (aFindPrev) {
          // We can continue at the end of mStartPointRange if its end is before
          // the start of returnRange or coincides with it.  Otherwise, we need
          // to continue at the start of returnRange.
          int16_t compareResult;
          nsresult rv =
            mStartPointRange->CompareBoundaryPoints(nsIDOMRange::START_TO_END,
                                                    returnRange, &compareResult);
          if (NS_SUCCEEDED(rv) && compareResult <= 0) {
            // OK to start at the end of mStartPointRange
            mStartPointRange->Collapse(false);
          } else {
            // Start at the beginning of returnRange
            returnRange->CloneRange(getter_AddRefs(mStartPointRange));
            mStartPointRange->Collapse(true);
          }
        } else {
          // We can continue at the start of mStartPointRange if its start is
          // after the end of returnRange or coincides with it.  Otherwise, we
          // need to continue at the end of returnRange.
          int16_t compareResult;
          nsresult rv =
            mStartPointRange->CompareBoundaryPoints(nsIDOMRange::END_TO_START,
                                                    returnRange, &compareResult);
          if (NS_SUCCEEDED(rv) && compareResult >= 0) {
            // OK to start at the start of mStartPointRange
            mStartPointRange->Collapse(true);
          } else {
            // Start at the end of returnRange
            returnRange->CloneRange(getter_AddRefs(mStartPointRange));
            mStartPointRange->Collapse(false);
          }
        }
        continue;
      }

      mFoundRange = returnRange;

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

      nsCOMPtr<nsPIDOMWindowInner> window = document->GetInnerWindow();
      NS_ASSERTION(window, "document has no window");
      if (!window)
        return NS_ERROR_UNEXPECTED;

      nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
      if (usesIndependentSelection) {
        /* If a search result is found inside an editable element, we'll focus
         * the element only if focus is in our content window, i.e.
         * |if (focusedWindow.top == ourWindow.top)| */
        bool shouldFocusEditableElement = false;
        if (fm) {
          nsCOMPtr<mozIDOMWindowProxy> focusedWindow;
          nsresult rv = fm->GetFocusedWindow(getter_AddRefs(focusedWindow));
          if (NS_SUCCEEDED(rv) && focusedWindow) {
            auto* fwPI = nsPIDOMWindowOuter::From(focusedWindow);
            nsCOMPtr<nsIDocShellTreeItem> fwTreeItem
              (do_QueryInterface(fwPI->GetDocShell(), &rv));
            if (NS_SUCCEEDED(rv)) {
              nsCOMPtr<nsIDocShellTreeItem> fwRootTreeItem;
              rv = fwTreeItem->GetSameTypeRootTreeItem(getter_AddRefs(fwRootTreeItem));
              if (NS_SUCCEEDED(rv) && fwRootTreeItem == rootContentTreeItem)
                shouldFocusEditableElement = true;
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
        fm->MoveFocus(window->GetOuterWindow(),
                      nullptr, nsIFocusManager::MOVEFOCUS_CARET,
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
          nsISelectionController::SELECTION_WHOLE_SELECTION,
          nsISelectionController::SCROLL_CENTER_VERTICALLY |
          nsISelectionController::SCROLL_SYNCHRONOUS);
      }

      mCurrentWindow = window;
      *aResult = hasWrapped ? FIND_WRAPPED : FIND_FOUND;
      return NS_OK;
    }

    // ======= end-inner-while (go through a single document) ==========

    // ---------- Nothing found yet, try next document  -------------
    bool hasTriedFirstDoc = false;
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
      hasTriedFirstDoc = true;      
    } while (docShellEnumerator);  // ==== end second inner while  ===

    bool continueLoop = false;
    if (currentDocShell != startingDocShell)
      continueLoop = true;  // Try next document
    else if (!hasWrapped || aIsFirstVisiblePreferred) {
      // Finished searching through docshells:
      // If aFirstVisiblePreferred == true, we may need to go through all
      // docshells twice -once to look for visible matches, the second time
      // for any match
      aIsFirstVisiblePreferred = false;
      hasWrapped = true;
      continueLoop = true; // Go through all docs again
    }

    if (continueLoop) {
      if (NS_FAILED(GetSearchContainers(currentContainer, nullptr,
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
        if (!mEndPointRange) {
          mEndPointRange = new nsRange(presShell->GetDocument());
        }

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
nsTypeAheadFind::GetCurrentWindow(mozIDOMWindow** aCurrentWindow)
{
  NS_ENSURE_ARG_POINTER(aCurrentWindow);
  *aCurrentWindow = mCurrentWindow;
  NS_IF_ADDREF(*aCurrentWindow);
  return NS_OK;
}

nsresult
nsTypeAheadFind::GetSearchContainers(nsISupports *aContainer,
                                     nsISelectionController *aSelectionController,
                                     bool aIsFirstVisiblePreferred,
                                     bool aFindPrev,
                                     nsIPresShell **aPresShell,
                                     nsPresContext **aPresContext)
{
  NS_ENSURE_ARG_POINTER(aContainer);
  NS_ENSURE_ARG_POINTER(aPresShell);
  NS_ENSURE_ARG_POINTER(aPresContext);

  *aPresShell = nullptr;
  *aPresContext = nullptr;

  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aContainer));
  if (!docShell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPresShell> presShell = docShell->GetPresShell();

  RefPtr<nsPresContext> presContext;
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

  if (!mSearchRange) {
    mSearchRange = new nsRange(doc);
  }
  nsCOMPtr<nsIDOMNode> searchRootNode = rootNode;

  // Hack for XMLPrettyPrinter. nsFind can't handle complex anonymous content.
  // If the root node has an XBL binding then there's not much we can do in
  // in general, but we can try searching the binding's first child, which
  // in the case of XMLPrettyPrinter contains the visible pretty-printed
  // content.
  nsXBLBinding* binding = rootContent->GetXBLBinding();
  if (binding) {
    nsIContent* anonContent = binding->GetAnonymousContent();
    if (anonContent) {
      searchRootNode = do_QueryInterface(anonContent->GetFirstChild());
    }
  }
  mSearchRange->SelectNodeContents(searchRootNode);

  if (!mStartPointRange) {
    mStartPointRange = new nsRange(doc);
  }
  mStartPointRange->SetStart(searchRootNode, 0);
  mStartPointRange->Collapse(true); // collapse to start

  if (!mEndPointRange) {
    mEndPointRange = new nsRange(doc);
  }
  nsCOMPtr<nsINode> searchRootTmp = do_QueryInterface(searchRootNode);
  mEndPointRange->SetEnd(searchRootNode, searchRootTmp->Length());
  mEndPointRange->Collapse(false); // collapse to end

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
                   aIsFirstVisiblePreferred, true, 
                   getter_AddRefs(mStartPointRange), nullptr);
  }
  else {
    int32_t startOffset;
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

  mStartPointRange->Collapse(true); // collapse to start

  presShell.forget(aPresShell);
  presContext.forget(aPresContext);

  return NS_OK;
}

void
nsTypeAheadFind::RangeStartsInsideLink(nsIDOMRange *aRange,
                                       nsIPresShell *aPresShell,
                                       bool *aIsInsideLink,
                                       bool *aIsStartingLink)
{
  *aIsInsideLink = false;
  *aIsStartingLink = true;

  // ------- Get nsIContent to test -------
  nsCOMPtr<nsIDOMNode> startNode;
  nsCOMPtr<nsIContent> startContent, origContent;
  aRange->GetStartContainer(getter_AddRefs(startNode));
  int32_t startOffset;
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
      for (int32_t index = 0; index < startOffset; index++) {
        // FIXME: take content language into account when deciding whitespace.
        if (!mozilla::dom::IsSpaceCharacter(textFrag->CharAt(index))) {
          *aIsStartingLink = false;  // not at start of a node

          break;
        }
      }
    }
  }

  // ------- Check to see if inside link ---------

  // We now have the correct start node for the range
  // Search for links, starting with startNode, and going up parent chain

  nsCOMPtr<nsIAtom> hrefAtom(NS_Atomize("href"));
  nsCOMPtr<nsIAtom> typeAtom(NS_Atomize("type"));

  while (true) {
    // Keep testing while startContent is equal to something,
    // eventually we'll run out of ancestors

    if (startContent->IsHTMLElement()) {
      nsCOMPtr<mozilla::dom::Link> link(do_QueryInterface(startContent));
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
          *aIsInsideLink = false;  // Xlink must be type="simple"
        }

        return;
      }
    }

    // Get the parent
    nsCOMPtr<nsIContent> parent = startContent->GetParent();
    if (!parent)
      break;

    nsIContent* parentsFirstChild = parent->GetFirstChild();

    // We don't want to look at a whitespace-only first child
    if (parentsFirstChild && parentsFirstChild->TextIsOnlyWhitespace()) {
      parentsFirstChild = parentsFirstChild->GetNextSibling();
    }

    if (parentsFirstChild != startContent) {
      // startContent wasn't a first child, so we conclude that
      // if this is inside a link, it's not at the beginning of it
      *aIsStartingLink = false;
    }

    startContent = parent;
  }

  *aIsStartingLink = false;
}

/* Find another match in the page. */
NS_IMETHODIMP
nsTypeAheadFind::FindAgain(bool aFindBackwards, bool aLinksOnly,
                           uint16_t* aResult)

{
  *aResult = FIND_NOTFOUND;

  if (!mTypeAheadBuffer.IsEmpty())
    // Beware! This may flush notifications via synchronous
    // ScrollSelectionIntoView.
    FindItNow(nullptr, aLinksOnly, false, aFindBackwards, aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsTypeAheadFind::Find(const nsAString& aSearchString, bool aLinksOnly,
                      uint16_t* aResult)
{
  *aResult = FIND_NOTFOUND;

  nsCOMPtr<nsIPresShell> presShell (GetPresShell());
  if (!presShell) {
    nsCOMPtr<nsIDocShell> ds (do_QueryReferent(mDocShell));
    NS_ENSURE_TRUE(ds, NS_ERROR_FAILURE);

    presShell = ds->GetPresShell();
    NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);
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
    mStartFindRange = nullptr;
    mSelectionController = nullptr;

    *aResult = FIND_FOUND;
    return NS_OK;
  }

  bool atEnd = false;
  if (mTypeAheadBuffer.Length()) {
    const nsAString& oldStr = Substring(mTypeAheadBuffer, 0, mTypeAheadBuffer.Length());
    const nsAString& newStr = Substring(aSearchString, 0, mTypeAheadBuffer.Length());
    if (oldStr.Equals(newStr))
      atEnd = true;

    const nsAString& newStr2 = Substring(aSearchString, 0, aSearchString.Length());
    const nsAString& oldStr2 = Substring(mTypeAheadBuffer, 0, aSearchString.Length());
    if (oldStr2.Equals(newStr2))
      atEnd = true;

    if (!atEnd)
      mStartFindRange = nullptr;
  }

  if (!mIsSoundInitialized && !mNotFoundSoundURL.IsEmpty()) {
    // This makes sure system sound library is loaded so that
    // there's no lag before the first sound is played
    // by waiting for the first keystroke, we still get the startup time benefits.
    mIsSoundInitialized = true;
    mSoundInterface = do_CreateInstance("@mozilla.org/sound;1");
    if (mSoundInterface && !mNotFoundSoundURL.EqualsLiteral("beep")) {
      mSoundInterface->Init();
    }
  }

#ifdef XP_WIN
  // After each keystroke, ensure sound object is destroyed, to free up memory 
  // allocated for error sound, otherwise Windows' nsISound impl 
  // holds onto the last played sound, using up memory.
  mSoundInterface = nullptr;
#endif

  int32_t bufferLength = mTypeAheadBuffer.Length();

  mTypeAheadBuffer = aSearchString;

  bool isFirstVisiblePreferred = false;

  // --------- Initialize find if 1st char ----------
  if (bufferLength == 0) {
    // If you can see the selection (not collapsed or thru caret browsing),
    // or if already focused on a page element, start there.
    // Otherwise we're going to start at the first visible element
    bool isSelectionCollapsed = true;
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

      nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
      if (fm) {
        nsPIDOMWindowOuter* window = document->GetWindow();
        nsCOMPtr<nsIDOMElement> focusedElement;
        nsCOMPtr<mozIDOMWindowProxy> focusedWindow;
        fm->GetFocusedElementForWindow(window, false,
                                       getter_AddRefs(focusedWindow),
                                       getter_AddRefs(focusedElement));
        // If the root element is focused, then it's actually the document
        // that has the focus, so ignore this.
        if (focusedElement &&
            !SameCOMIdentity(focusedElement, document->GetRootElement())) {
          fm->MoveCaretToFocus(window);
          isFirstVisiblePreferred = false;
        }
      }
    }
  }

  // ----------- Find the text! ---------------------
  // Beware! This may flush notifications via synchronous
  // ScrollSelectionIntoView.
  nsresult rv = FindItNow(nullptr, aLinksOnly, isFirstVisiblePreferred,
                          false, aResult);

  // ---------Handle success or failure ---------------
  if (NS_SUCCEEDED(rv)) {
    if (mTypeAheadBuffer.Length() == 1) {
      // If first letter, store where the first find succeeded
      // (mStartFindRange)

      mStartFindRange = nullptr;
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

  // if aCurrentNode is nullptr, get selection for document
  *aDOMSel = nullptr;

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

NS_IMETHODIMP
nsTypeAheadFind::GetFoundRange(nsIDOMRange** aFoundRange)
{
  NS_ENSURE_ARG_POINTER(aFoundRange);
  if (mFoundRange == nullptr) {
    *aFoundRange = nullptr;
    return NS_OK;
  }

  mFoundRange->CloneRange(aFoundRange);
  return NS_OK;
}

NS_IMETHODIMP
nsTypeAheadFind::IsRangeVisible(nsIDOMRange *aRange,
                                bool aMustBeInViewPort,
                                bool *aResult)
{
  // Jump through hoops to extract the docShell from the range.
  nsCOMPtr<nsIDOMNode> node;
  aRange->GetStartContainer(getter_AddRefs(node));
  nsCOMPtr<nsIDOMDocument> document;
  node->GetOwnerDocument(getter_AddRefs(document));
  nsCOMPtr<mozIDOMWindowProxy> window;
  document->GetDefaultView(getter_AddRefs(window));
  nsCOMPtr<nsIWebNavigation> navNav (do_GetInterface(window));
  nsCOMPtr<nsIDocShell> docShell (do_GetInterface(navNav));

  // Set up the arguments needed to check if a range is visible.
  nsCOMPtr<nsIPresShell> presShell (docShell->GetPresShell());
  RefPtr<nsPresContext> presContext = presShell->GetPresContext();
  nsCOMPtr<nsIDOMRange> startPointRange = new nsRange(presShell->GetDocument());
  *aResult = IsRangeVisible(presShell, presContext, aRange,
                            aMustBeInViewPort, false,
                            getter_AddRefs(startPointRange),
                            nullptr);
  return NS_OK;
}

bool
nsTypeAheadFind::IsRangeVisible(nsIPresShell *aPresShell,
                                nsPresContext *aPresContext,
                                nsIDOMRange *aRange, bool aMustBeInViewPort,
                                bool aGetTopVisibleLeaf,
                                nsIDOMRange **aFirstVisibleRange,
                                bool *aUsesIndependentSelection)
{
  NS_ASSERTION(aPresShell && aPresContext && aRange && aFirstVisibleRange, 
               "params are invalid");

  // We need to know if the range start is visible.
  // Otherwise, return the first visible range start 
  // in aFirstVisibleRange

  aRange->CloneRange(aFirstVisibleRange);
  nsCOMPtr<nsIDOMNode> node;
  aRange->GetStartContainer(getter_AddRefs(node));

  nsCOMPtr<nsIContent> content(do_QueryInterface(node));
  if (!content)
    return false;

  nsIFrame *frame = content->GetPrimaryFrame();
  if (!frame)    
    return false;  // No frame! Not visible then.

  if (!frame->StyleVisibility()->IsVisible())
    return false;

  // Detect if we are _inside_ a text control, or something else with its own
  // selection controller.
  if (aUsesIndependentSelection) {
    *aUsesIndependentSelection = 
      (frame->GetStateBits() & NS_FRAME_INDEPENDENT_SELECTION);
  }

  // ---- We have a frame ----
  if (!aMustBeInViewPort)   
    return true; //  Don't need it to be on screen, just in rendering tree

  // Get the next in flow frame that contains the range start
  int32_t startRangeOffset, startFrameOffset, endFrameOffset;
  aRange->GetStartOffset(&startRangeOffset);
  while (true) {
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
  const uint16_t kMinPixels  = 12;
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
      return true;
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
                            false, // aVisual
                            false, // aLockInScrollView
                            false, // aFollowOOFs
                            false  // aSkipPopupChecks
                            );

  if (!frameTraversal)
    return false;

  while (rectVisibility == nsRectVisibility_kAboveViewport) {
    frameTraversal->Next();
    frame = frameTraversal->CurrentItem();
    if (!frame)
      return false;

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
      frame->GetOffsets(startFrameOffset, endFrameOffset);
      (*aFirstVisibleRange)->SetStart(firstVisibleNode, startFrameOffset);
      (*aFirstVisibleRange)->SetEnd(firstVisibleNode, endFrameOffset);
    }
  }

  return false;
}

NS_IMETHODIMP
nsTypeAheadFind::IsRangeRendered(nsIDOMRange *aRange,
                                bool *aResult)
{
  // Jump through hoops to extract the docShell from the range.
  nsCOMPtr<nsIDOMNode> node;
  aRange->GetStartContainer(getter_AddRefs(node));
  nsCOMPtr<nsIDOMDocument> document;
  node->GetOwnerDocument(getter_AddRefs(document));
  nsCOMPtr<mozIDOMWindowProxy> window;
  document->GetDefaultView(getter_AddRefs(window));
  nsCOMPtr<nsIWebNavigation> navNav (do_GetInterface(window));
  nsCOMPtr<nsIDocShell> docShell (do_GetInterface(navNav));

  // Set up the arguments needed to check if a range is visible.
  nsCOMPtr<nsIPresShell> presShell (docShell->GetPresShell());
  RefPtr<nsPresContext> presContext = presShell->GetPresContext();
  *aResult = IsRangeRendered(presShell, presContext, aRange);
  return NS_OK;
}

bool
nsTypeAheadFind::IsRangeRendered(nsIPresShell *aPresShell,
                                 nsPresContext *aPresContext,
                                 nsIDOMRange *aRange)
{
  NS_ASSERTION(aPresShell && aPresContext && aRange,
               "params are invalid");

  nsCOMPtr<nsIDOMNode> node;
  aRange->GetCommonAncestorContainer(getter_AddRefs(node));

  nsCOMPtr<nsIContent> content(do_QueryInterface(node));
  if (!content) {
    return false;
  }

  nsIFrame *frame = content->GetPrimaryFrame();
  if (!frame) {
    return false;  // No frame! Not visible then.
  }

  if (!frame->StyleVisibility()->IsVisible()) {
    return false;
  }

  // Having a primary frame doesn't mean that the range is visible inside the
  // viewport. Do a hit-test to determine that quickly and properly.
  AutoTArray<nsIFrame*,8> frames;
  nsIFrame *rootFrame = aPresShell->GetRootFrame();
  RefPtr<nsRange> range = static_cast<nsRange*>(aRange);
  RefPtr<mozilla::dom::DOMRectList> rects = range->GetClientRects(true, false);
  for (uint32_t i = 0; i < rects->Length(); ++i) {
    RefPtr<mozilla::dom::DOMRect> rect = rects->Item(i);
    nsRect r(nsPresContext::CSSPixelsToAppUnits((float)rect->X()),
             nsPresContext::CSSPixelsToAppUnits((float)rect->Y()),
             nsPresContext::CSSPixelsToAppUnits((float)rect->Width()),
             nsPresContext::CSSPixelsToAppUnits((float)rect->Height()));
    // Append visible frames to frames array.
    nsLayoutUtils::GetFramesForArea(rootFrame, r, frames,
      nsLayoutUtils::IGNORE_PAINT_SUPPRESSION |
      nsLayoutUtils::IGNORE_ROOT_SCROLL_FRAME |
      nsLayoutUtils::ONLY_VISIBLE);

    // See if any of the frames contain the content. If they do, then the range
    // is visible. We search for the content rather than the original frame,
    // because nsTextContinuation frames might be returned instead of the
    // original frame.
    for (const auto &f: frames) {
      if (f->GetContent() == content) {
        return true;
      }
    }

    frames.ClearAndRetainStorage();
  }

  return false;
}

already_AddRefed<nsIPresShell>
nsTypeAheadFind::GetPresShell()
{
  if (!mPresShell)
    return nullptr;

  nsCOMPtr<nsIPresShell> shell = do_QueryReferent(mPresShell);
  if (shell) {
    nsPresContext *pc = shell->GetPresContext();
    if (!pc || !pc->GetContainerWeak()) {
      return nullptr;
    }
  }

  return shell.forget();
}
