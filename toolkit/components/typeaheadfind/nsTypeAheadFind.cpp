/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsMemory.h"
#include "nsIServiceManager.h"
#include "mozilla/ErrorResult.h"
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
#include "nsGenericHTMLElement.h"

#include "nsIDOMNode.h"
#include "nsIFrame.h"
#include "nsFrameTraversal.h"
#include "nsIImageDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIContent.h"
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
#include "mozilla/dom/RangeBinding.h"
#include "mozilla/dom/Selection.h"
#include "nsRange.h"
#include "nsXBLBinding.h"

#include "nsTypeAheadFind.h"

using namespace mozilla;
using namespace mozilla::dom;

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
    rv = prefInternal->AddObserver("accessibility.typeaheadfind", this, true);
    NS_ENSURE_SUCCESS(rv, rv);

    // ----------- Get initial preferences ----------
    PrefsReset();

    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->AddObserver(this, DOM_WINDOW_DESTROYED_TOPIC, true);
    }
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
  nsAutoCString soundStr;
  if (isSoundEnabled)
    prefBranch->GetCharPref("accessibility.typeaheadfind.soundURL", soundStr);

  mNotFoundSoundURL = soundStr;

  if (!mNotFoundSoundURL.IsEmpty() && !mNotFoundSoundURL.EqualsLiteral("beep")) {
    if (!mSoundInterface) {
      mSoundInterface = do_CreateInstance("@mozilla.org/sound;1");
    }

    // Init to load the system sound library if the lib is not ready
    if (mSoundInterface) {
      mIsSoundInitialized = true;
      mSoundInterface->Init();
    }
  }

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

  RefPtr<Selection> selection;
  nsCOMPtr<nsISelectionController> selectionController =
    do_QueryReferent(mSelectionController);
  if (!selectionController) {
    GetSelection(presShell, getter_AddRefs(selectionController),
                 getter_AddRefs(selection)); // cache for reuse
    mSelectionController = do_GetWeakReference(selectionController);
  } else {
    selection = selectionController->GetDOMSelection(
      nsISelectionController::SELECTION_NORMAL);
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
  RefPtr<nsRange> returnRange;
  if (NS_FAILED(GetSearchContainers(currentContainer,
                                    (!aIsFirstVisiblePreferred ||
                                     mStartFindRange) ?
                                    selectionController.get() : nullptr,
                                    aIsFirstVisiblePreferred,  aFindPrev,
                                    getter_AddRefs(presShell),
                                    getter_AddRefs(presContext)))) {
    return NS_ERROR_FAILURE;
  }

  if (!mStartPointRange) {
    mStartPointRange = new nsRange(presShell->GetDocument());
  }

  // XXXbz Should this really be ignoring errors?
  int16_t rangeCompareResult =
    mStartPointRange->CompareBoundaryPoints(RangeBinding::START_TO_START,
                                            *mSearchRange, IgnoreErrors());
  // No need to wrap find in doc if starting at beginning
  bool hasWrapped = (rangeCompareResult < 0);

  if (mTypeAheadBuffer.IsEmpty() || !EnsureFind())
    return NS_ERROR_FAILURE;

  mFind->SetFindBackwards(aFindPrev);

  while (true) {    // ----- Outer while loop: go through all docs -----
    while (true) {  // === Inner while loop: go through a single doc ===
      nsCOMPtr<nsIDOMRange> tempFoundRange;
      mFind->Find(mTypeAheadBuffer.get(), mSearchRange, mStartPointRange,
                  mEndPointRange, getter_AddRefs(tempFoundRange));
      returnRange = static_cast<nsRange*>(tempFoundRange.get());
      if (!returnRange) {
        break;  // Nothing found in this doc, go to outer loop (try next doc)
      }

      // ------- Test resulting found range for success conditions ------
      bool isInsideLink = false, isStartingLink = false;

      if (aIsLinksOnly) {
        // Don't check if inside link when searching all text
        RangeStartsInsideLink(returnRange, presShell,
                              &isInsideLink, &isStartingLink);
      }

      bool usesIndependentSelection;
      // Check actual visibility of the range, and generate some
      // side effects (like updating mStartPointRange and
      // setting usesIndependentSelection) that we'll need whether
      // or not the range is visible.
      bool canSeeRange = IsRangeVisible(presShell, presContext,
                                        returnRange,
                                        aIsFirstVisiblePreferred, false,
                                        getter_AddRefs(mStartPointRange),
                                        &usesIndependentSelection);

      // If we can't see the range, we still might be able to scroll
      // it into view if usesIndependentSelection is true. If both are
      // false, then we treat it as a failure condition.
      if ((!canSeeRange && !usesIndependentSelection) ||
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
          IgnoredErrorResult rv;
          int16_t compareResult =
            mStartPointRange->CompareBoundaryPoints(RangeBinding::START_TO_END,
                                                    *returnRange, rv);
          if (!rv.Failed() && compareResult <= 0) {
            // OK to start at the end of mStartPointRange
            mStartPointRange->Collapse(false);
          } else {
            // Start at the beginning of returnRange
            mStartPointRange = returnRange->CloneRange();
            mStartPointRange->Collapse(true);
          }
        } else {
          // We can continue at the start of mStartPointRange if its start is
          // after the end of returnRange or coincides with it.  Otherwise, we
          // need to continue at the end of returnRange.
          IgnoredErrorResult rv;
          int16_t compareResult =
            mStartPointRange->CompareBoundaryPoints(RangeBinding::END_TO_START,
                                                    *returnRange, rv);
          if (!rv.Failed() && compareResult >= 0) {
            // OK to start at the start of mStartPointRange
            mStartPointRange->Collapse(true);
          } else {
            // Start at the end of returnRange
            mStartPointRange = returnRange->CloneRange();
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
        nsCOMPtr<nsINode> node = returnRange->GetStartContainer();
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
              selection = selectionController->GetDOMSelection(
                nsISelectionController::SELECTION_NORMAL);
            }
            mFoundEditable = do_QueryInterface(node);

            if (!shouldFocusEditableElement)
              break;

            // Otherwise move focus/caret to editable element
            if (fm)
              fm->SetFocus(mFoundEditable, 0);
            break;
          }
          node = node->GetParentNode();
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
        selection->RemoveAllRanges(IgnoreErrors());
        selection->AddRange(*returnRange, IgnoreErrors());
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
        RefPtr<nsRange> tempRange = mStartPointRange->CloneRange();
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
nsTypeAheadFind::GetFoundLink(Element** aFoundLink)
{
  NS_ENSURE_ARG_POINTER(aFoundLink);
  *aFoundLink = mFoundLink;
  NS_IF_ADDREF(*aFoundLink);
  return NS_OK;
}

NS_IMETHODIMP
nsTypeAheadFind::GetFoundEditable(Element** aFoundEditable)
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
  if (doc->IsHTMLOrXHTML()) {
    rootContent = doc->GetBody();
  }

  if (!rootContent) {
    rootContent = doc->GetRootElement();
    if (!rootContent) {
      return NS_ERROR_FAILURE;
    }
  }

  if (!mSearchRange) {
    mSearchRange = new nsRange(doc);
  }
  nsCOMPtr<nsINode> searchRootNode(rootContent);

  // Hack for XMLPrettyPrinter. nsFind can't handle complex anonymous content.
  // If the root node has an XBL binding then there's not much we can do in
  // in general, but we can try searching the binding's first child, which
  // in the case of XMLPrettyPrinter contains the visible pretty-printed
  // content.
  nsXBLBinding* binding = rootContent->GetXBLBinding();
  if (binding) {
    nsIContent* anonContent = binding->GetAnonymousContent();
    if (anonContent) {
      searchRootNode = anonContent->GetFirstChild();
    }
  }
  mSearchRange->SelectNodeContents(*searchRootNode, IgnoreErrors());

  if (!mStartPointRange) {
    mStartPointRange = new nsRange(doc);
  }
  mStartPointRange->SetStart(*searchRootNode, 0, IgnoreErrors());
  mStartPointRange->Collapse(true); // collapse to start

  if (!mEndPointRange) {
    mEndPointRange = new nsRange(doc);
  }
  mEndPointRange->SetEnd(*searchRootNode, searchRootNode->Length(), IgnoreErrors());
  mEndPointRange->Collapse(false); // collapse to end

  // Consider current selection as null if
  // it's not in the currently focused document
  RefPtr<nsRange> currentSelectionRange;
  nsCOMPtr<nsIPresShell> selectionPresShell = GetPresShell();
  if (aSelectionController && selectionPresShell && selectionPresShell == presShell) {
    RefPtr<Selection> selection = aSelectionController->GetDOMSelection(
      nsISelectionController::SELECTION_NORMAL);
    if (selection) {
      currentSelectionRange = selection->GetRangeAt(0);
    }
  }

  if (!currentSelectionRange) {
    // Ensure visible range, move forward if necessary
    // This ignores the return value, but uses the side effect of
    // IsRangeVisible. It returns the first visible range after searchRange
    IsRangeVisible(presShell, presContext, mSearchRange,
                   aIsFirstVisiblePreferred, true,
                   getter_AddRefs(mStartPointRange), nullptr);
  }
  else {
    uint32_t startOffset;
    nsCOMPtr<nsINode> startNode;
    if (aFindPrev) {
      startNode = currentSelectionRange->GetStartContainer();
      startOffset = currentSelectionRange->StartOffset();
    } else {
      startNode = currentSelectionRange->GetEndContainer();
      startOffset = currentSelectionRange->EndOffset();
    }

    if (!startNode) {
      startNode = rootContent;
    }

    // We need to set the start point this way, other methods haven't worked
    mStartPointRange->SelectNode(*startNode, IgnoreErrors());
    mStartPointRange->SetStart(*startNode, startOffset, IgnoreErrors());
  }

  mStartPointRange->Collapse(true); // collapse to start

  presShell.forget(aPresShell);
  presContext.forget(aPresContext);

  return NS_OK;
}

void
nsTypeAheadFind::RangeStartsInsideLink(nsRange *aRange,
                                       nsIPresShell *aPresShell,
                                       bool *aIsInsideLink,
                                       bool *aIsStartingLink)
{
  *aIsInsideLink = false;
  *aIsStartingLink = true;

  // ------- Get nsIContent to test -------
  uint32_t startOffset = aRange->StartOffset();

  nsCOMPtr<nsIContent> startContent =
    do_QueryInterface(aRange->GetStartContainer());
  if (!startContent) {
    NS_NOTREACHED("startContent should never be null");
    return;
  }
  nsCOMPtr<nsIContent> origContent = startContent;

  if (startContent->IsElement()) {
    nsIContent *childContent = aRange->GetChildAtStartOffset();
    if (childContent) {
      startContent = childContent;
    }
  }
  else if (startOffset > 0) {
    const nsTextFragment *textFrag = startContent->GetText();
    if (textFrag) {
      // look for non whitespace character before start offset
      for (uint32_t index = 0; index < startOffset; index++) {
        // FIXME: take content language into account when deciding whitespace.
        if (!mozilla::dom::IsSpaceCharacter(
                             textFrag->CharAt(static_cast<int32_t>(index)))) {
          *aIsStartingLink = false;  // not at start of a node

          break;
        }
      }
    }
  }

  // ------- Check to see if inside link ---------

  // We now have the correct start node for the range
  // Search for links, starting with startNode, and going up parent chain

  while (true) {
    // Keep testing while startContent is equal to something,
    // eventually we'll run out of ancestors

    if (startContent->IsHTMLElement()) {
      nsCOMPtr<mozilla::dom::Link> link(do_QueryInterface(startContent));
      if (link) {
        // Check to see if inside HTML link
        *aIsInsideLink = startContent->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::href);
        return;
      }
    } else {
      // Any xml element can be an xlink
      *aIsInsideLink = startContent->IsElement() &&
        startContent->AsElement()->HasAttr(kNameSpaceID_XLink, nsGkAtoms::href);
      if (*aIsInsideLink) {
        if (!startContent->AsElement()->AttrValueIs(kNameSpaceID_XLink,
                                                    nsGkAtoms::type,
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

  RefPtr<Selection> selection;
  nsCOMPtr<nsISelectionController> selectionController =
    do_QueryReferent(mSelectionController);
  if (!selectionController) {
    GetSelection(presShell, getter_AddRefs(selectionController),
                 getter_AddRefs(selection)); // cache for reuse
    mSelectionController = do_GetWeakReference(selectionController);
  } else {
    selection = selectionController->GetDOMSelection(
      nsISelectionController::SELECTION_NORMAL);
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
        RefPtr<Element> focusedElement;
        nsCOMPtr<mozIDOMWindowProxy> focusedWindow;
        fm->GetFocusedElementForWindow(window, false,
                                       getter_AddRefs(focusedWindow),
                                       getter_AddRefs(focusedElement));
        // If the root element is focused, then it's actually the document
        // that has the focus, so ignore this.
        if (focusedElement && focusedElement != document->GetRootElement()) {
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
        RefPtr<nsRange> startFindRange = selection->GetRangeAt(0);
        if (startFindRange) {
          mStartFindRange = startFindRange->CloneRange();
        }
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
                              Selection **aDOMSel)
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
      RefPtr<Selection> sel =
        (*aSelCon)->GetDOMSelection(nsISelectionController::SELECTION_NORMAL);
      sel.forget(aDOMSel);
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

  *aFoundRange = mFoundRange->CloneRange().take();
  return NS_OK;
}

NS_IMETHODIMP
nsTypeAheadFind::IsRangeVisible(nsIDOMRange *aRange,
                                bool aMustBeInViewPort,
                                bool *aResult)
{
  nsRange* range = static_cast<nsRange*>(aRange);
  nsCOMPtr<nsINode> node = range->GetStartContainer();

  nsIDocument* doc = node->OwnerDoc();
  nsCOMPtr<nsIPresShell> presShell = doc->GetShell();
  if (!presShell) {
    return NS_ERROR_UNEXPECTED;
  }
  RefPtr<nsPresContext> presContext = presShell->GetPresContext();
  RefPtr<nsRange> ignored;
  *aResult = IsRangeVisible(presShell, presContext, range,
                            aMustBeInViewPort, false,
                            getter_AddRefs(ignored),
                            nullptr);
  return NS_OK;
}

bool
nsTypeAheadFind::IsRangeVisible(nsIPresShell *aPresShell,
                                nsPresContext *aPresContext,
                                nsRange *aRange, bool aMustBeInViewPort,
                                bool aGetTopVisibleLeaf,
                                nsRange **aFirstVisibleRange,
                                bool *aUsesIndependentSelection)
{
  NS_ASSERTION(aPresShell && aPresContext && aRange && aFirstVisibleRange,
               "params are invalid");

  // We need to know if the range start is visible.
  // Otherwise, return the first visible range start
  // in aFirstVisibleRange

  *aFirstVisibleRange = aRange->CloneRange().take();

  nsCOMPtr<nsIContent> content =
    do_QueryInterface(aRange->GetStartContainer());
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

  // Detect if we are _inside_ a text control, or something else with its own
  // selection controller.
  if (aUsesIndependentSelection) {
    *aUsesIndependentSelection =
      (frame->GetStateBits() & NS_FRAME_INDEPENDENT_SELECTION);
  }

  // ---- We have a frame ----

  // Get the next in flow frame that contains the range start
  int32_t startFrameOffset, endFrameOffset;
  uint32_t startRangeOffset = aRange->StartOffset();
  while (true) {
    frame->GetOffsets(startFrameOffset, endFrameOffset);
    if (static_cast<int32_t>(startRangeOffset) < endFrameOffset) {
      break;
    }

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

    if (rectVisibility == nsRectVisibility_kVisible) {
      // This is an early exit case, where we return true if and only if
      // the range is actually rendered.
      return IsRangeRendered(aPresShell, aPresContext, aRange);
    }
  }

  // Below this point, we know the range is not in the viewport.

  if (!aMustBeInViewPort) {
    // This is an early exit case because we don't care that that range
    // is out of viewport, so we return that the range is "visible".
    return true;
  }

  // The range isn't in the viewport, but we could scroll it into view.
  // Move range forward to first visible point,
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

  if (!frameTraversal) {
    return false;
  }

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
    nsINode* firstVisibleNode = frame->GetContent();

    if (firstVisibleNode) {
      frame->GetOffsets(startFrameOffset, endFrameOffset);
      (*aFirstVisibleRange)->SetStart(*firstVisibleNode, startFrameOffset,
                                      IgnoreErrors());
      (*aFirstVisibleRange)->SetEnd(*firstVisibleNode, endFrameOffset,
                                    IgnoreErrors());
    }
  }

  return false;
}

NS_IMETHODIMP
nsTypeAheadFind::IsRangeRendered(nsIDOMRange *aRange,
                                bool *aResult)
{
  nsRange* range = static_cast<nsRange*>(aRange);
  nsINode* node = range->GetStartContainer();

  nsIDocument* doc = node->OwnerDoc();
  nsCOMPtr<nsIPresShell> presShell = doc->GetShell();
  if (!presShell) {
    return NS_ERROR_UNEXPECTED;
  }
  RefPtr<nsPresContext> presContext = presShell->GetPresContext();
  *aResult = IsRangeRendered(presShell, presContext, range);
  return NS_OK;
}

bool
nsTypeAheadFind::IsRangeRendered(nsIPresShell *aPresShell,
                                 nsPresContext *aPresContext,
                                 nsRange *aRange)
{
  NS_ASSERTION(aPresShell && aPresContext && aRange,
               "params are invalid");

  nsCOMPtr<nsIContent> content =
    do_QueryInterface(aRange->GetCommonAncestor());
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
  RefPtr<mozilla::dom::DOMRectList> rects = range->GetClientRects(true, true);
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
