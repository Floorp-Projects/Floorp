/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsDocShell.h"
#include "nsMemory.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/ModuleUtils.h"
#include "mozilla/PresShell.h"
#include "mozilla/Services.h"
#include "nsCURILoader.h"
#include "nsCycleCollectionParticipant.h"
#include "nsNetUtil.h"
#include "nsIURL.h"
#include "nsIURI.h"
#include "nsIDocShell.h"
#include "nsISimpleEnumerator.h"
#include "nsPIDOMWindow.h"
#include "nsIPrefBranch.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsGenericHTMLElement.h"

#include "nsIFrame.h"
#include "nsContainerFrame.h"
#include "nsFrameTraversal.h"
#include "mozilla/dom/Document.h"
#include "nsIContent.h"
#include "nsTextFragment.h"
#include "nsIEditor.h"

#include "nsIDocShellTreeItem.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsContentCID.h"
#include "nsLayoutCID.h"
#include "nsWidgetsCID.h"
#include "nsIFormControl.h"
#include "nsNameSpaceManager.h"
#include "nsIObserverService.h"
#include "nsFocusManager.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/HTMLTextAreaElement.h"
#include "mozilla/dom/Link.h"
#include "mozilla/dom/RangeBinding.h"
#include "mozilla/dom/Selection.h"
#include "nsLayoutUtils.h"
#include "nsRange.h"

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

NS_IMPL_CYCLE_COLLECTION_WEAK(nsTypeAheadFind, mFoundLink, mFoundEditable,
                              mCurrentWindow, mStartFindRange, mSearchRange,
                              mStartPointRange, mEndPointRange, mSoundInterface,
                              mFind, mFoundRange)

#define NS_FIND_CONTRACTID "@mozilla.org/embedcomp/rangefind;1"

nsTypeAheadFind::nsTypeAheadFind()
    : mStartLinksOnlyPref(false),
      mCaretBrowsingOn(false),
      mDidAddObservers(false),
      mLastFindLength(0),
      mIsSoundInitialized(false),
      mCaseSensitive(false),
      mEntireWord(false),
      mMatchDiacritics(false) {}

nsTypeAheadFind::~nsTypeAheadFind() {
  nsCOMPtr<nsIPrefBranch> prefInternal(
      do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefInternal) {
    prefInternal->RemoveObserver("accessibility.typeaheadfind", this);
    prefInternal->RemoveObserver("accessibility.browsewithcaret", this);
  }
}

nsresult nsTypeAheadFind::Init(nsIDocShell* aDocShell) {
  nsCOMPtr<nsIPrefBranch> prefInternal(
      do_GetService(NS_PREFSERVICE_CONTRACTID));

  mSearchRange = nullptr;
  mStartPointRange = nullptr;
  mEndPointRange = nullptr;
  if (!prefInternal || !EnsureFind()) return NS_ERROR_FAILURE;

  SetDocShell(aDocShell);

  if (!mDidAddObservers) {
    mDidAddObservers = true;
    // ----------- Listen to prefs ------------------
    nsresult rv =
        prefInternal->AddObserver("accessibility.browsewithcaret", this, true);
    NS_ENSURE_SUCCESS(rv, rv);
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
    // by waiting for the first keystroke, we still get the startup time
    // benefits.
    mIsSoundInitialized = true;
    mSoundInterface = do_CreateInstance("@mozilla.org/sound;1");
    if (mSoundInterface && !mNotFoundSoundURL.EqualsLiteral("beep")) {
      mSoundInterface->Init();
    }
  }

  return NS_OK;
}

nsresult nsTypeAheadFind::PrefsReset() {
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

  if (!mNotFoundSoundURL.IsEmpty() &&
      !mNotFoundSoundURL.EqualsLiteral("beep")) {
    if (!mSoundInterface) {
      mSoundInterface = do_CreateInstance("@mozilla.org/sound;1");
    }

    // Init to load the system sound library if the lib is not ready
    if (mSoundInterface) {
      mIsSoundInitialized = true;
      mSoundInterface->Init();
    }
  }

  prefBranch->GetBoolPref("accessibility.browsewithcaret", &mCaretBrowsingOn);

  return NS_OK;
}

NS_IMETHODIMP
nsTypeAheadFind::SetCaseSensitive(bool isCaseSensitive) {
  mCaseSensitive = isCaseSensitive;

  if (mFind) {
    mFind->SetCaseSensitive(mCaseSensitive);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTypeAheadFind::GetCaseSensitive(bool* isCaseSensitive) {
  *isCaseSensitive = mCaseSensitive;

  return NS_OK;
}

NS_IMETHODIMP
nsTypeAheadFind::SetEntireWord(bool isEntireWord) {
  mEntireWord = isEntireWord;

  if (mFind) {
    mFind->SetEntireWord(mEntireWord);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTypeAheadFind::GetEntireWord(bool* isEntireWord) {
  *isEntireWord = mEntireWord;

  return NS_OK;
}

NS_IMETHODIMP
nsTypeAheadFind::SetMatchDiacritics(bool matchDiacritics) {
  mMatchDiacritics = matchDiacritics;

  if (mFind) {
    mFind->SetMatchDiacritics(mMatchDiacritics);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTypeAheadFind::GetMatchDiacritics(bool* matchDiacritics) {
  *matchDiacritics = mMatchDiacritics;

  return NS_OK;
}

NS_IMETHODIMP
nsTypeAheadFind::SetDocShell(nsIDocShell* aDocShell) {
  mDocShell = do_GetWeakReference(aDocShell);

  mWebBrowserFind = do_GetInterface(aDocShell);
  NS_ENSURE_TRUE(mWebBrowserFind, NS_ERROR_FAILURE);

  mDocument = do_GetWeakReference(aDocShell->GetExtantDocument());

  ReleaseStrongMemberVariables();
  return NS_OK;
}

void nsTypeAheadFind::ReleaseStrongMemberVariables() {
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
nsTypeAheadFind::SetSelectionModeAndRepaint(int16_t aToggle) {
  nsCOMPtr<nsISelectionController> selectionController =
      do_QueryReferent(mSelectionController);
  if (!selectionController) {
    return NS_OK;
  }

  selectionController->SetDisplaySelection(aToggle);
  selectionController->RepaintSelection(
      nsISelectionController::SELECTION_NORMAL);

  return NS_OK;
}

MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHODIMP nsTypeAheadFind::CollapseSelection() {
  nsCOMPtr<nsISelectionController> selectionController =
      do_QueryReferent(mSelectionController);
  if (!selectionController) {
    return NS_OK;
  }

  RefPtr<Selection> selection = selectionController->GetSelection(
      nsISelectionController::SELECTION_NORMAL);
  if (selection) {
    selection->CollapseToStart(IgnoreErrors());
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTypeAheadFind::Observe(nsISupports* aSubject, const char* aTopic,
                         const char16_t* aData) {
  if (!nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    return PrefsReset();
  }
  if (!nsCRT::strcmp(aTopic, DOM_WINDOW_DESTROYED_TOPIC) &&
      SameCOMIdentity(aSubject, mCurrentWindow)) {
    ReleaseStrongMemberVariables();
  }

  return NS_OK;
}

void nsTypeAheadFind::SaveFind() {
  if (mWebBrowserFind) mWebBrowserFind->SetSearchString(mTypeAheadBuffer);

  // save the length of this find for "not found" sound
  mLastFindLength = mTypeAheadBuffer.Length();
}

void nsTypeAheadFind::PlayNotFoundSound() {
  if (mNotFoundSoundURL.IsEmpty())  // no sound
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
      NS_NewURI(getter_AddRefs(soundURI),
                nsLiteralCString(TYPEAHEADFIND_NOTFOUND_WAV_URL));
    else
      NS_NewURI(getter_AddRefs(soundURI), mNotFoundSoundURL);

    nsCOMPtr<nsIURL> soundURL(do_QueryInterface(soundURI));
    if (soundURL) mSoundInterface->Play(soundURL);
  }
}

nsresult nsTypeAheadFind::FindItNow(uint32_t aMode, bool aIsLinksOnly,
                                    bool aIsFirstVisiblePreferred,
                                    bool aDontIterateFrames,
                                    uint16_t* aResult) {
  *aResult = FIND_NOTFOUND;
  mFoundLink = nullptr;
  mFoundEditable = nullptr;
  mFoundRange = nullptr;
  mCurrentWindow = nullptr;
  RefPtr<Document> startingDocument = GetDocument();
  NS_ENSURE_TRUE(startingDocument, NS_ERROR_FAILURE);

  // There could be unflushed notifications which hide textareas or other
  // elements that we don't want to find text in.
  startingDocument->FlushPendingNotifications(mozilla::FlushType::Layout);

  RefPtr<PresShell> presShell = startingDocument->GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);
  RefPtr<nsPresContext> presContext = presShell->GetPresContext();
  NS_ENSURE_TRUE(presContext, NS_ERROR_FAILURE);

  RefPtr<Selection> selection;
  nsCOMPtr<nsISelectionController> selectionController =
      do_QueryReferent(mSelectionController);
  if (!selectionController) {
    GetSelection(presShell, getter_AddRefs(selectionController),
                 getter_AddRefs(selection));  // cache for reuse
    mSelectionController = do_GetWeakReference(selectionController);
  } else {
    selection = selectionController->GetSelection(
        nsISelectionController::SELECTION_NORMAL);
  }

  nsCOMPtr<nsIDocShell> startingDocShell(presContext->GetDocShell());
  NS_ASSERTION(
      startingDocShell,
      "Bug 175321 Crashes with Type Ahead Find [@ nsTypeAheadFind::FindItNow]");
  if (!startingDocShell) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocShell> currentDocShell;
  nsCOMPtr<nsISupports> currentContainer;
  nsCOMPtr<nsIDocShellTreeItem> rootContentTreeItem;
  nsCOMPtr<nsIDocShell> rootContentDocShell;
  typedef nsTArray<RefPtr<nsIDocShell>> DocShells;
  DocShells docShells;
  DocShells::const_iterator it, it_end;
  if (!aDontIterateFrames) {
    // The use of GetInProcessSameTypeRootTreeItem (and later in this method) is
    // OK here as out-of-process frames are handled externally by
    // FinderParent.jsm, which will end up only calling this method with
    // aDontIterateFrames set to true.
    startingDocShell->GetInProcessSameTypeRootTreeItem(
        getter_AddRefs(rootContentTreeItem));
    rootContentDocShell = do_QueryInterface(rootContentTreeItem);

    if (!rootContentDocShell) return NS_ERROR_FAILURE;

    rootContentDocShell->GetAllDocShellsInSubtree(
        nsIDocShellTreeItem::typeContent, nsIDocShell::ENUMERATE_FORWARDS,
        docShells);

    // Default: can start at the current document
    currentContainer = do_QueryInterface(rootContentDocShell);

    // Iterate up to current shell, if there's more than 1 that we're
    // dealing with
    for (it = docShells.begin(), it_end = docShells.end(); it != it_end; ++it) {
      currentDocShell = *it;
      if (!currentDocShell || currentDocShell == startingDocShell ||
          aIsFirstVisiblePreferred)
        break;
    }
  } else {
    currentContainer = currentDocShell = startingDocShell;
  }

  bool findPrev = (aMode == FIND_PREVIOUS || aMode == FIND_LAST);

  // ------------ Get ranges ready ----------------

  bool useSelection = (aMode != FIND_FIRST && aMode != FIND_LAST) &&
                      (!aIsFirstVisiblePreferred || mStartFindRange);

  RefPtr<nsRange> returnRange;
  if (NS_FAILED(GetSearchContainers(
          currentContainer, useSelection ? selectionController.get() : nullptr,
          aIsFirstVisiblePreferred, findPrev, getter_AddRefs(presShell),
          getter_AddRefs(presContext)))) {
    return NS_ERROR_FAILURE;
  }

  if (!mStartPointRange) {
    mStartPointRange = nsRange::Create(presShell->GetDocument());
  }

  // XXXbz Should this really be ignoring errors?
  int16_t rangeCompareResult = mStartPointRange->CompareBoundaryPoints(
      Range_Binding::START_TO_START, *mSearchRange, IgnoreErrors());
  // No need to wrap find in doc if starting at beginning
  bool hasWrapped = (rangeCompareResult < 0);

  if (mTypeAheadBuffer.IsEmpty() || !EnsureFind()) return NS_ERROR_FAILURE;

  mFind->SetFindBackwards(findPrev);

  while (true) {    // ----- Outer while loop: go through all docs -----
    while (true) {  // === Inner while loop: go through a single doc ===
      mFind->Find(mTypeAheadBuffer, mSearchRange, mStartPointRange,
                  mEndPointRange, getter_AddRefs(returnRange));
      if (!returnRange) {
        break;  // Nothing found in this doc, go to outer loop (try next doc)
      }

      // ------- Test resulting found range for success conditions ------
      bool isInsideLink = false, isStartingLink = false;

      if (aIsLinksOnly) {
        // Don't check if inside link when searching all text
        RangeStartsInsideLink(returnRange, &isInsideLink, &isStartingLink);
      }

      bool usesIndependentSelection = false;
      // Check actual visibility of the range, and generate some
      // side effects (like updating mStartPointRange and
      // setting usesIndependentSelection) that we'll need whether
      // or not the range is visible.
      bool canSeeRange = IsRangeVisible(returnRange, aIsFirstVisiblePreferred,
                                        false, &usesIndependentSelection);

      mStartPointRange = returnRange->CloneRange();

      // If we can't see the range, we still might be able to scroll
      // it into view if usesIndependentSelection is true. If both are
      // false, then we treat it as a failure condition.
      if ((!canSeeRange && !usesIndependentSelection) ||
          (aIsLinksOnly && !isInsideLink) ||
          (mStartLinksOnlyPref && aIsLinksOnly && !isStartingLink)) {
        // We want to jump over this range, so collapse to the start if we're
        // finding backwards and vice versa.
        mStartPointRange->Collapse(findPrev);
        continue;
      }

      mFoundRange = returnRange;

      // ------ Success! -------
      // Hide old selection (new one may be on a different controller)
      if (selection) {
        selection->CollapseToStart(IgnoreErrors());
        SetSelectionModeAndRepaint(nsISelectionController::SELECTION_ON);
      }

      RefPtr<Document> document = presShell->GetDocument();
      NS_ASSERTION(document, "Wow, presShell doesn't have document!");
      if (!document) {
        return NS_ERROR_UNEXPECTED;
      }

      // Make sure new document is selected
      if (document != startingDocument) {
        // We are in a new document (because of frames/iframes)
        mDocument = do_GetWeakReference(document);
      }

      nsCOMPtr<nsPIDOMWindowInner> window = document->GetInnerWindow();
      NS_ASSERTION(window, "document has no window");
      if (!window) return NS_ERROR_UNEXPECTED;

      RefPtr<nsFocusManager> fm = nsFocusManager::GetFocusManager();
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
            nsCOMPtr<nsIDocShellTreeItem> fwTreeItem(fwPI->GetDocShell());
            if (NS_SUCCEEDED(rv)) {
              nsCOMPtr<nsIDocShellTreeItem> fwRootTreeItem;
              rv = fwTreeItem->GetInProcessSameTypeRootTreeItem(
                  getter_AddRefs(fwRootTreeItem));
              if (NS_SUCCEEDED(rv) && fwRootTreeItem == rootContentTreeItem)
                shouldFocusEditableElement = true;
            }
          }
        }

        // We may be inside an editable element, and therefore the selection
        // may be controlled by a different selection controller.  Walk up the
        // chain of parent nodes to see if we find one.
        nsINode* node = returnRange->GetStartContainer();
        while (node) {
          nsCOMPtr<nsIEditor> editor;
          if (RefPtr<HTMLInputElement> input =
                  HTMLInputElement::FromNode(node)) {
            editor = input->GetTextEditor();
          } else if (RefPtr<HTMLTextAreaElement> textarea =
                         HTMLTextAreaElement::FromNode(node)) {
            editor = textarea->GetTextEditor();
          } else {
            node = node->GetParentNode();
            continue;
          }

          // Inside an editable element.  Get the correct selection
          // controller and selection.
          NS_ASSERTION(editor, "Editable element has no editor!");
          if (!editor) {
            break;
          }
          editor->GetSelectionController(getter_AddRefs(selectionController));
          if (selectionController) {
            selection = selectionController->GetSelection(
                nsISelectionController::SELECTION_NORMAL);
          }
          mFoundEditable = node->AsElement();

          if (!shouldFocusEditableElement) {
            break;
          }

          // Otherwise move focus/caret to editable element
          if (fm) {
            nsCOMPtr<Element> newFocusElement = mFoundEditable;
            fm->SetFocus(newFocusElement, 0);
          }
          break;
        }

        // If we reach here without setting mFoundEditable, then something
        // besides editable elements gave us an independent selection
        // controller. List controls with multiple visible elements can do
        // this (nsAreaSelectsFrame), and possibly others. We fall back to
        // grabbing the document's selection controller in this case.
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
        selection->AddRangeAndSelectFramesAndNotifyListeners(*returnRange,
                                                             IgnoreErrors());
      }

      if (!mFoundEditable && fm) {
        fm->MoveFocus(window->GetOuterWindow(), nullptr,
                      nsIFocusManager::MOVEFOCUS_CARET,
                      nsIFocusManager::FLAG_NOSCROLL |
                          nsIFocusManager::FLAG_NOSWITCHFRAME,
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
    if (aDontIterateFrames) {
      return NS_OK;
    }

    // ---------- Nothing found yet, try next document  -------------
    bool hasTriedFirstDoc = false;
    do {
      // ==== Second inner loop - get another while  ====
      if (it != it_end) {
        currentContainer = *it;
        ++it;
        NS_ASSERTION(currentContainer, "We're not at the end yet!");
        currentDocShell = do_QueryInterface(currentContainer);

        if (currentDocShell) break;
      } else if (hasTriedFirstDoc)  // Avoid potential infinite loop
        return NS_ERROR_FAILURE;    // No content doc shells

      // Reached last doc shell, loop around back to first doc shell
      rootContentDocShell->GetAllDocShellsInSubtree(
          nsIDocShellTreeItem::typeContent, nsIDocShell::ENUMERATE_FORWARDS,
          docShells);
      it = docShells.begin();
      it_end = docShells.end();
      hasTriedFirstDoc = true;
    } while (it != it_end);  // ==== end second inner while  ===

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
      continueLoop = true;  // Go through all docs again
    }

    if (continueLoop) {
      if (NS_FAILED(GetSearchContainers(
              currentContainer, nullptr, aIsFirstVisiblePreferred, findPrev,
              getter_AddRefs(presShell), getter_AddRefs(presContext)))) {
        continue;
      }

      if (findPrev) {
        // Reverse mode: swap start and end points, so that we start
        // at end of document and go to beginning
        RefPtr<nsRange> tempRange = mStartPointRange->CloneRange();
        if (!mEndPointRange) {
          mEndPointRange = nsRange::Create(presShell->GetDocument());
        }

        mStartPointRange = mEndPointRange;
        mEndPointRange = tempRange;
      }

      continue;
    }

    // ------------- Failed --------------
    break;
  }  // end-outer-while: go through all docs

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsTypeAheadFind::GetSearchString(nsAString& aSearchString) {
  aSearchString = mTypeAheadBuffer;
  return NS_OK;
}

NS_IMETHODIMP
nsTypeAheadFind::GetFoundLink(Element** aFoundLink) {
  NS_ENSURE_ARG_POINTER(aFoundLink);
  *aFoundLink = mFoundLink;
  NS_IF_ADDREF(*aFoundLink);
  return NS_OK;
}

NS_IMETHODIMP
nsTypeAheadFind::GetFoundEditable(Element** aFoundEditable) {
  NS_ENSURE_ARG_POINTER(aFoundEditable);
  *aFoundEditable = mFoundEditable;
  NS_IF_ADDREF(*aFoundEditable);
  return NS_OK;
}

NS_IMETHODIMP
nsTypeAheadFind::GetCurrentWindow(mozIDOMWindow** aCurrentWindow) {
  NS_ENSURE_ARG_POINTER(aCurrentWindow);
  *aCurrentWindow = mCurrentWindow;
  NS_IF_ADDREF(*aCurrentWindow);
  return NS_OK;
}

nsresult nsTypeAheadFind::GetSearchContainers(
    nsISupports* aContainer, nsISelectionController* aSelectionController,
    bool aIsFirstVisiblePreferred, bool aFindPrev, PresShell** aPresShell,
    nsPresContext** aPresContext) {
  NS_ENSURE_ARG_POINTER(aContainer);
  NS_ENSURE_ARG_POINTER(aPresShell);
  NS_ENSURE_ARG_POINTER(aPresContext);

  *aPresShell = nullptr;
  *aPresContext = nullptr;

  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aContainer));
  if (!docShell) return NS_ERROR_FAILURE;

  RefPtr<PresShell> presShell = docShell->GetPresShell();

  RefPtr<nsPresContext> presContext = docShell->GetPresContext();

  if (!presShell || !presContext) return NS_ERROR_FAILURE;

  Document* doc = presShell->GetDocument();

  if (!doc) return NS_ERROR_FAILURE;

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
    mSearchRange = nsRange::Create(doc);
  }
  nsCOMPtr<nsINode> searchRootNode(rootContent);

  mSearchRange->SelectNodeContents(*searchRootNode, IgnoreErrors());

  if (!mStartPointRange) {
    mStartPointRange = nsRange::Create(doc);
  }
  mStartPointRange->SetStartAndEnd(searchRootNode, 0, searchRootNode, 0);

  if (!mEndPointRange) {
    mEndPointRange = nsRange::Create(doc);
  }
  mEndPointRange->SetStartAndEnd(searchRootNode, searchRootNode->Length(),
                                 searchRootNode, searchRootNode->Length());

  // Consider current selection as null if
  // it's not in the currently focused document
  RefPtr<const nsRange> currentSelectionRange;
  RefPtr<Document> selectionDocument = GetDocument();
  if (aSelectionController && selectionDocument && selectionDocument == doc) {
    RefPtr<Selection> selection = aSelectionController->GetSelection(
        nsISelectionController::SELECTION_NORMAL);
    if (selection) {
      currentSelectionRange = selection->GetRangeAt(0);
    }
  }

  if (!currentSelectionRange) {
    mStartPointRange = mSearchRange->CloneRange();
    // We want to search in the visible selection range. That means that the
    // start point needs to be the end if we're looking backwards, or vice
    // versa.
    mStartPointRange->Collapse(!aFindPrev);
  } else {
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
    mStartPointRange->Collapse(true);  // collapse to start
  }

  presShell.forget(aPresShell);
  presContext.forget(aPresContext);

  return NS_OK;
}

void nsTypeAheadFind::RangeStartsInsideLink(nsRange* aRange,
                                            bool* aIsInsideLink,
                                            bool* aIsStartingLink) {
  *aIsInsideLink = false;
  *aIsStartingLink = true;

  // ------- Get nsIContent to test -------
  uint32_t startOffset = aRange->StartOffset();

  nsCOMPtr<nsIContent> startContent =
      nsIContent::FromNodeOrNull(aRange->GetStartContainer());
  if (!startContent) {
    MOZ_ASSERT_UNREACHABLE("startContent should never be null");
    return;
  }
  nsCOMPtr<nsIContent> origContent = startContent;

  if (startContent->IsElement()) {
    nsIContent* childContent = aRange->GetChildAtStartOffset();
    if (childContent) {
      startContent = childContent;
    }
  } else if (startOffset > 0) {
    const nsTextFragment* textFrag = startContent->GetText();
    if (textFrag) {
      // look for non whitespace character before start offset
      for (uint32_t index = 0; index < startOffset; index++) {
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

  while (true) {
    // Keep testing while startContent is equal to something,
    // eventually we'll run out of ancestors

    if (startContent->IsHTMLElement()) {
      nsCOMPtr<mozilla::dom::Link> link(do_QueryInterface(startContent));
      if (link) {
        // Check to see if inside HTML link
        *aIsInsideLink = startContent->AsElement()->HasAttr(kNameSpaceID_None,
                                                            nsGkAtoms::href);
        return;
      }
    } else {
      // Any xml element can be an xlink
      *aIsInsideLink =
          startContent->IsElement() && startContent->AsElement()->HasAttr(
                                           kNameSpaceID_XLink, nsGkAtoms::href);
      if (*aIsInsideLink) {
        if (!startContent->AsElement()->AttrValueIs(
                kNameSpaceID_XLink, nsGkAtoms::type, u"simple"_ns,
                eCaseMatters)) {
          *aIsInsideLink = false;  // Xlink must be type="simple"
        }

        return;
      }
    }

    // Get the parent
    nsCOMPtr<nsIContent> parent = startContent->GetParent();
    if (!parent) break;

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

MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHODIMP nsTypeAheadFind::Find(
    const nsAString& aSearchString, bool aLinksOnly, uint32_t aMode,
    bool aDontIterateFrames, uint16_t* aResult) {
  if (aMode == nsITypeAheadFind::FIND_PREVIOUS ||
      aMode == nsITypeAheadFind::FIND_NEXT) {
    if (mTypeAheadBuffer.IsEmpty()) {
      *aResult = FIND_NOTFOUND;
    } else {
      FindItNow(aMode, aLinksOnly, false, aDontIterateFrames, aResult);
    }

    return NS_OK;
  }

  // Find again ignores error return values, so do so here as well.
  nsresult rv = FindInternal(aMode, aSearchString, aLinksOnly,
                             aDontIterateFrames, aResult);
  return (aMode == nsITypeAheadFind::FIND_INITIAL) ? rv : NS_OK;
}

nsresult nsTypeAheadFind::FindInternal(uint32_t aMode,
                                       const nsAString& aSearchString,
                                       bool aLinksOnly, bool aDontIterateFrames,
                                       uint16_t* aResult) {
  *aResult = FIND_NOTFOUND;

  RefPtr<Document> doc = GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  RefPtr<PresShell> presShell = doc->GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  RefPtr<Selection> selection;
  nsCOMPtr<nsISelectionController> selectionController =
      do_QueryReferent(mSelectionController);
  if (!selectionController) {
    GetSelection(presShell, getter_AddRefs(selectionController),
                 getter_AddRefs(selection));  // cache for reuse
    mSelectionController = do_GetWeakReference(selectionController);
  } else {
    selection = selectionController->GetSelection(
        nsISelectionController::SELECTION_NORMAL);
  }

  if (selection) {
    selection->CollapseToStart(IgnoreErrors());
  }

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
  bool isInitial = aMode == nsITypeAheadFind::FIND_INITIAL;
  if (isInitial) {
    if (mTypeAheadBuffer.Length()) {
      const nsAString& oldStr =
          Substring(mTypeAheadBuffer, 0, mTypeAheadBuffer.Length());
      const nsAString& newStr =
          Substring(aSearchString, 0, mTypeAheadBuffer.Length());
      if (oldStr.Equals(newStr)) atEnd = true;

      const nsAString& newStr2 =
          Substring(aSearchString, 0, aSearchString.Length());
      const nsAString& oldStr2 =
          Substring(mTypeAheadBuffer, 0, aSearchString.Length());
      if (oldStr2.Equals(newStr2)) atEnd = true;

      if (!atEnd) mStartFindRange = nullptr;
    }
  }

  int32_t bufferLength = mTypeAheadBuffer.Length();

  mTypeAheadBuffer = aSearchString;

  bool isFirstVisiblePreferred = false;

  // --------- Initialize find if 1st char ----------
  if (bufferLength == 0 && isInitial) {
    // If you can see the selection (not collapsed or thru caret browsing),
    // or if already focused on a page element, start there.
    // Otherwise we're going to start at the first visible element
    bool isSelectionCollapsed = !selection || selection->IsCollapsed();

    // If true, we will scan from top left of visible area
    // If false, we will scan from start of selection
    isFirstVisiblePreferred =
        !atEnd && !mCaretBrowsingOn && isSelectionCollapsed;
    if (isFirstVisiblePreferred) {
      // Get the focused content. If there is a focused node, ensure the
      // selection is at that point. Otherwise, we will just want to start
      // from the caret position or the beginning of the document.
      nsPresContext* presContext = presShell->GetPresContext();
      NS_ENSURE_TRUE(presContext, NS_OK);

      nsCOMPtr<Document> document = presShell->GetDocument();
      if (!document) return NS_ERROR_UNEXPECTED;

      nsFocusManager* fm = nsFocusManager::GetFocusManager();
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
  nsresult rv = FindItNow(aMode, aLinksOnly, isFirstVisiblePreferred,
                          aDontIterateFrames, aResult);

  // ---------Handle success or failure ---------------
  if (NS_SUCCEEDED(rv)) {
    if (mTypeAheadBuffer.Length() == 1) {
      // If first letter, store where the first find succeeded
      // (mStartFindRange)

      mStartFindRange = nullptr;
      if (selection) {
        RefPtr<const nsRange> startFindRange = selection->GetRangeAt(0);
        if (startFindRange) {
          mStartFindRange = startFindRange->CloneRange();
        }
      }
    }
  } else if (isInitial) {
    // Error sound, except when whole word matching is ON.
    if (!mEntireWord && mTypeAheadBuffer.Length() > mLastFindLength)
      PlayNotFoundSound();
  }

  SaveFind();
  return NS_OK;
}

void nsTypeAheadFind::GetSelection(PresShell* aPresShell,
                                   nsISelectionController** aSelCon,
                                   Selection** aDOMSel) {
  if (!aPresShell) return;

  // if aCurrentNode is nullptr, get selection for document
  *aDOMSel = nullptr;

  nsPresContext* presContext = aPresShell->GetPresContext();

  nsIFrame* frame = aPresShell->GetRootFrame();

  if (presContext && frame) {
    frame->GetSelectionController(presContext, aSelCon);
    if (*aSelCon) {
      RefPtr<Selection> sel =
          (*aSelCon)->GetSelection(nsISelectionController::SELECTION_NORMAL);
      sel.forget(aDOMSel);
    }
  }
}

NS_IMETHODIMP
nsTypeAheadFind::GetFoundRange(nsRange** aFoundRange) {
  NS_ENSURE_ARG_POINTER(aFoundRange);
  if (mFoundRange == nullptr) {
    *aFoundRange = nullptr;
    return NS_OK;
  }

  *aFoundRange = mFoundRange->CloneRange().take();
  return NS_OK;
}

NS_IMETHODIMP
nsTypeAheadFind::IsRangeVisible(nsRange* aRange, bool aMustBeInViewPort,
                                bool* aResult) {
  *aResult = IsRangeVisible(aRange, aMustBeInViewPort, false, nullptr);
  return NS_OK;
}

bool nsTypeAheadFind::IsRangeVisible(nsRange* aRange, bool aMustBeInViewPort,
                                     bool aGetTopVisibleLeaf,
                                     bool* aUsesIndependentSelection) {
  // We need to know if the range start is visible.
  // Otherwise, return the first visible range start in aFirstVisibleRange
  nsCOMPtr<nsIContent> content =
      nsIContent::FromNodeOrNull(aRange->GetStartContainer());
  if (!content) {
    return false;
  }

  nsIFrame* frame = content->GetPrimaryFrame();
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

  return aMustBeInViewPort ? IsRangeRendered(aRange) : true;
}

NS_IMETHODIMP
nsTypeAheadFind::IsRangeRendered(nsRange* aRange, bool* aResult) {
  *aResult = IsRangeRendered(aRange);
  return NS_OK;
}

bool nsTypeAheadFind::IsRangeRendered(nsRange* aRange) {
  using FrameForPointOption = nsLayoutUtils::FrameForPointOption;
  nsCOMPtr<nsIContent> content =
      nsIContent::FromNodeOrNull(aRange->GetClosestCommonInclusiveAncestor());
  if (!content) {
    return false;
  }

  nsIFrame* frame = content->GetPrimaryFrame();
  if (!frame) {
    return false;  // No frame! Not visible then.
  }

  if (!frame->StyleVisibility()->IsVisible()) {
    return false;
  }

  // Having a primary frame doesn't mean that the range is visible inside the
  // viewport. Do a hit-test to determine that quickly and properly.
  AutoTArray<nsIFrame*, 8> frames;
  nsIFrame* rootFrame = frame->PresShell()->GetRootFrame();
  RefPtr<nsRange> range = static_cast<nsRange*>(aRange);

  // NOTE(emilio): This used to flush layout, _after_ checking style above.
  // Instead, don't flush.
  RefPtr<mozilla::dom::DOMRectList> rects =
      range->GetClientRects(true, /* aFlushLayout = */ false);
  for (uint32_t i = 0; i < rects->Length(); ++i) {
    RefPtr<mozilla::dom::DOMRect> rect = rects->Item(i);
    nsRect r(nsPresContext::CSSPixelsToAppUnits((float)rect->X()),
             nsPresContext::CSSPixelsToAppUnits((float)rect->Y()),
             nsPresContext::CSSPixelsToAppUnits((float)rect->Width()),
             nsPresContext::CSSPixelsToAppUnits((float)rect->Height()));
    // Append visible frames to frames array.
    nsLayoutUtils::GetFramesForArea(
        RelativeTo{rootFrame}, r, frames,
        {{FrameForPointOption::IgnorePaintSuppression,
          FrameForPointOption::IgnoreRootScrollFrame,
          FrameForPointOption::OnlyVisible}});

    // See if any of the frames contain the content. If they do, then the range
    // is visible. We search for the content rather than the original frame,
    // because nsTextContinuation frames might be returned instead of the
    // original frame.
    for (const auto& f : frames) {
      if (f->GetContent() == content) {
        return true;
      }
    }

    frames.ClearAndRetainStorage();
  }

  return false;
}

already_AddRefed<Document> nsTypeAheadFind::GetDocument() {
  // Try the last document we found and ensure it's sane.
  RefPtr<Document> doc = do_QueryReferent(mDocument);
  if (doc && doc->GetPresShell() && doc->GetDocShell()) {
    return doc.forget();
  }

  // Otherwise fall back to the document from which we were initialized (the one
  // from mDocShell).
  mDocument = nullptr;
  nsCOMPtr<nsIDocShell> ds = do_QueryReferent(mDocShell);
  if (!ds) {
    return nullptr;
  }
  doc = ds->GetExtantDocument();
  mDocument = do_GetWeakReference(doc);
  return doc.forget();
}
