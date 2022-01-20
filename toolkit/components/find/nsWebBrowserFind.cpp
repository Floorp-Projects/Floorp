/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWebBrowserFind.h"

// Only need this for NS_FIND_CONTRACTID,
// else we could use nsRange.h and nsIFind.h.
#include "nsFind.h"

#include "mozilla/dom/ScriptSettings.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsPIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsPresContext.h"
#include "mozilla/dom/Document.h"
#include "nsISelectionController.h"
#include "nsIFrame.h"
#include "nsITextControlFrame.h"
#include "nsReadableUtils.h"
#include "nsIContent.h"
#include "nsContentCID.h"
#include "nsIObserverService.h"
#include "nsISupportsPrimitives.h"
#include "nsFind.h"
#include "nsError.h"
#include "nsFocusManager.h"
#include "nsRange.h"
#include "mozilla/PresShell.h"
#include "mozilla/Services.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Selection.h"
#include "nsISimpleEnumerator.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsGenericHTMLElement.h"

#if DEBUG
#  include "nsIWebNavigation.h"
#  include "nsString.h"
#endif

using mozilla::dom::Document;
using mozilla::dom::Element;
using mozilla::dom::Selection;

nsWebBrowserFind::nsWebBrowserFind()
    : mFindBackwards(false),
      mWrapFind(false),
      mEntireWord(false),
      mMatchCase(false),
      mMatchDiacritics(false),
      mSearchSubFrames(true),
      mSearchParentFrames(true) {}

nsWebBrowserFind::~nsWebBrowserFind() = default;

NS_IMPL_ISUPPORTS(nsWebBrowserFind, nsIWebBrowserFind,
                  nsIWebBrowserFindInFrames)

NS_IMETHODIMP
nsWebBrowserFind::FindNext(bool* aResult) {
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = false;

  NS_ENSURE_TRUE(CanFindNext(), NS_ERROR_NOT_INITIALIZED);

  nsresult rv = NS_OK;
  nsCOMPtr<nsPIDOMWindowOuter> searchFrame =
      do_QueryReferent(mCurrentSearchFrame);
  NS_ENSURE_TRUE(searchFrame, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsPIDOMWindowOuter> rootFrame = do_QueryReferent(mRootSearchFrame);
  NS_ENSURE_TRUE(rootFrame, NS_ERROR_NOT_INITIALIZED);

  // first, if there's a "cmd_findagain" observer around, check to see if it
  // wants to perform the find again command . If it performs the find again
  // it will return true, in which case we exit ::FindNext() early.
  // Otherwise, nsWebBrowserFind needs to perform the find again command itself
  // this is used by nsTypeAheadFind, which controls find again when it was
  // the last executed find in the current window.
  nsCOMPtr<nsIObserverService> observerSvc =
      mozilla::services::GetObserverService();
  if (observerSvc) {
    nsCOMPtr<nsISupportsInterfacePointer> windowSupportsData =
        do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsISupports> searchWindowSupports = do_QueryInterface(rootFrame);
    windowSupportsData->SetData(searchWindowSupports);
    observerSvc->NotifyObservers(windowSupportsData,
                                 "nsWebBrowserFind_FindAgain",
                                 mFindBackwards ? u"up" : u"down");
    windowSupportsData->GetData(getter_AddRefs(searchWindowSupports));
    // findnext performed if search window data cleared out
    *aResult = searchWindowSupports == nullptr;
    if (*aResult) {
      return NS_OK;
    }
  }

  // next, look in the current frame. If found, return.

  // Beware! This may flush notifications via synchronous
  // ScrollSelectionIntoView.
  rv = SearchInFrame(searchFrame, false, aResult);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (*aResult) {
    return OnFind(searchFrame);  // we are done
  }

  // if we are not searching other frames, return
  if (!mSearchSubFrames && !mSearchParentFrames) {
    return NS_OK;
  }

  nsIDocShell* rootDocShell = rootFrame->GetDocShell();
  if (!rootDocShell) {
    return NS_ERROR_FAILURE;
  }

  auto enumDirection = mFindBackwards ? nsIDocShell::ENUMERATE_BACKWARDS
                                      : nsIDocShell::ENUMERATE_FORWARDS;

  nsTArray<RefPtr<nsIDocShell>> docShells;
  rv = rootDocShell->GetAllDocShellsInSubtree(nsIDocShellTreeItem::typeAll,
                                              enumDirection, docShells);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // remember where we started
  nsCOMPtr<nsIDocShellTreeItem> startingItem = searchFrame->GetDocShell();

  // XXX We should avoid searching in frameset documents here.
  // We also need to honour mSearchSubFrames and mSearchParentFrames.
  bool doFind = false;
  for (const auto& curItem : docShells) {
    if (doFind) {
      searchFrame = curItem->GetWindow();
      if (!searchFrame) {
        break;
      }

      OnStartSearchFrame(searchFrame);

      // Beware! This may flush notifications via synchronous
      // ScrollSelectionIntoView.
      rv = SearchInFrame(searchFrame, false, aResult);
      if (NS_FAILED(rv)) {
        return rv;
      }
      if (*aResult) {
        return OnFind(searchFrame);  // we are done
      }

      OnEndSearchFrame(searchFrame);
    }

    if (curItem.get() == startingItem.get()) {
      doFind = true;  // start looking in frames after this one
    }
  }

  if (!mWrapFind) {
    // remember where we left off
    SetCurrentSearchFrame(searchFrame);
    return NS_OK;
  }

  // From here on, we're wrapping, first through the other frames, then finally
  // from the beginning of the starting frame back to the starting point.

  // because nsISimpleEnumerator is bad and isn't resettable, I have to
  // make a new one
  rv = rootDocShell->GetAllDocShellsInSubtree(nsIDocShellTreeItem::typeAll,
                                              enumDirection, docShells);
  if (NS_FAILED(rv)) {
    return rv;
  }

  for (const auto& curItem : docShells) {
    searchFrame = curItem->GetWindow();
    if (!searchFrame) {
      rv = NS_ERROR_FAILURE;
      break;
    }

    if (curItem.get() == startingItem.get()) {
      // Beware! This may flush notifications via synchronous
      // ScrollSelectionIntoView.
      rv = SearchInFrame(searchFrame, true, aResult);
      if (NS_FAILED(rv)) {
        return rv;
      }
      if (*aResult) {
        return OnFind(searchFrame);  // we are done
      }
      break;
    }

    OnStartSearchFrame(searchFrame);

    // Beware! This may flush notifications via synchronous
    // ScrollSelectionIntoView.
    rv = SearchInFrame(searchFrame, false, aResult);
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (*aResult) {
      return OnFind(searchFrame);  // we are done
    }

    OnEndSearchFrame(searchFrame);
  }

  // remember where we left off
  SetCurrentSearchFrame(searchFrame);

  NS_ASSERTION(NS_SUCCEEDED(rv), "Something failed");
  return rv;
}

NS_IMETHODIMP
nsWebBrowserFind::GetSearchString(nsAString& aSearchString) {
  aSearchString = mSearchString;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::SetSearchString(const nsAString& aSearchString) {
  mSearchString = aSearchString;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::GetFindBackwards(bool* aFindBackwards) {
  NS_ENSURE_ARG_POINTER(aFindBackwards);
  *aFindBackwards = mFindBackwards;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::SetFindBackwards(bool aFindBackwards) {
  mFindBackwards = aFindBackwards;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::GetWrapFind(bool* aWrapFind) {
  NS_ENSURE_ARG_POINTER(aWrapFind);
  *aWrapFind = mWrapFind;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::SetWrapFind(bool aWrapFind) {
  mWrapFind = aWrapFind;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::GetEntireWord(bool* aEntireWord) {
  NS_ENSURE_ARG_POINTER(aEntireWord);
  *aEntireWord = mEntireWord;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::SetEntireWord(bool aEntireWord) {
  mEntireWord = aEntireWord;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::GetMatchCase(bool* aMatchCase) {
  NS_ENSURE_ARG_POINTER(aMatchCase);
  *aMatchCase = mMatchCase;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::SetMatchCase(bool aMatchCase) {
  mMatchCase = aMatchCase;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::GetMatchDiacritics(bool* aMatchDiacritics) {
  NS_ENSURE_ARG_POINTER(aMatchDiacritics);
  *aMatchDiacritics = mMatchDiacritics;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::SetMatchDiacritics(bool aMatchDiacritics) {
  mMatchDiacritics = aMatchDiacritics;
  return NS_OK;
}

void nsWebBrowserFind::SetSelectionAndScroll(nsPIDOMWindowOuter* aWindow,
                                             nsRange* aRange) {
  RefPtr<Document> doc = aWindow->GetDoc();
  if (!doc) {
    return;
  }

  PresShell* presShell = doc->GetPresShell();
  if (!presShell) {
    return;
  }

  nsCOMPtr<nsINode> node = aRange->GetStartContainer();
  nsCOMPtr<nsIContent> content(do_QueryInterface(node));
  nsIFrame* frame = content->GetPrimaryFrame();
  if (!frame) {
    return;
  }
  nsCOMPtr<nsISelectionController> selCon;
  frame->GetSelectionController(presShell->GetPresContext(),
                                getter_AddRefs(selCon));

  // since the match could be an anonymous textnode inside a
  // <textarea> or text <input>, we need to get the outer frame
  nsITextControlFrame* tcFrame = nullptr;
  for (; content; content = content->GetParent()) {
    if (!content->IsInNativeAnonymousSubtree()) {
      nsIFrame* f = content->GetPrimaryFrame();
      if (!f) {
        return;
      }
      tcFrame = do_QueryFrame(f);
      break;
    }
  }

  selCon->SetDisplaySelection(nsISelectionController::SELECTION_ON);
  RefPtr<Selection> selection =
      selCon->GetSelection(nsISelectionController::SELECTION_NORMAL);
  if (selection) {
    selection->RemoveAllRanges(IgnoreErrors());
    selection->AddRangeAndSelectFramesAndNotifyListeners(*aRange,
                                                         IgnoreErrors());

    if (RefPtr<nsFocusManager> fm = nsFocusManager::GetFocusManager()) {
      if (tcFrame) {
        RefPtr<Element> newFocusedElement = Element::FromNode(content);
        fm->SetFocus(newFocusedElement, nsIFocusManager::FLAG_NOSCROLL);
      } else {
        RefPtr<Element> result;
        fm->MoveFocus(aWindow, nullptr, nsIFocusManager::MOVEFOCUS_CARET,
                      nsIFocusManager::FLAG_NOSCROLL, getter_AddRefs(result));
      }
    }

    // Scroll if necessary to make the selection visible:
    // Must be the last thing to do - bug 242056

    // After ScrollSelectionIntoView(), the pending notifications might be
    // flushed and PresShell/PresContext/Frames may be dead. See bug 418470.
    selCon->ScrollSelectionIntoView(
        nsISelectionController::SELECTION_NORMAL,
        nsISelectionController::SELECTION_WHOLE_SELECTION,
        nsISelectionController::SCROLL_CENTER_VERTICALLY |
            nsISelectionController::SCROLL_SYNCHRONOUS);
  }
}

// Adapted from TextServicesDocument::GetDocumentContentRootNode
nsresult nsWebBrowserFind::GetRootNode(Document* aDoc, Element** aNode) {
  NS_ENSURE_ARG_POINTER(aDoc);
  NS_ENSURE_ARG_POINTER(aNode);
  *aNode = 0;

  if (aDoc->IsHTMLOrXHTML()) {
    Element* body = aDoc->GetBody();
    NS_ENSURE_ARG_POINTER(body);
    NS_ADDREF(*aNode = body);
    return NS_OK;
  }

  // For non-HTML documents, the content root node will be the doc element.
  Element* root = aDoc->GetDocumentElement();
  NS_ENSURE_ARG_POINTER(root);
  NS_ADDREF(*aNode = root);
  return NS_OK;
}

nsresult nsWebBrowserFind::SetRangeAroundDocument(nsRange* aSearchRange,
                                                  nsRange* aStartPt,
                                                  nsRange* aEndPt,
                                                  Document* aDoc) {
  RefPtr<Element> bodyContent;
  nsresult rv = GetRootNode(aDoc, getter_AddRefs(bodyContent));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_ARG_POINTER(bodyContent);

  uint32_t childCount = bodyContent->GetChildCount();

  aSearchRange->SetStart(*bodyContent, 0, IgnoreErrors());
  aSearchRange->SetEnd(*bodyContent, childCount, IgnoreErrors());

  if (mFindBackwards) {
    aStartPt->SetStart(*bodyContent, childCount, IgnoreErrors());
    aStartPt->SetEnd(*bodyContent, childCount, IgnoreErrors());
    aEndPt->SetStart(*bodyContent, 0, IgnoreErrors());
    aEndPt->SetEnd(*bodyContent, 0, IgnoreErrors());
  } else {
    aStartPt->SetStart(*bodyContent, 0, IgnoreErrors());
    aStartPt->SetEnd(*bodyContent, 0, IgnoreErrors());
    aEndPt->SetStart(*bodyContent, childCount, IgnoreErrors());
    aEndPt->SetEnd(*bodyContent, childCount, IgnoreErrors());
  }

  return NS_OK;
}

// Set the range to go from the end of the current selection to the end of the
// document (forward), or beginning to beginning (reverse). or around the whole
// document if there's no selection.
nsresult nsWebBrowserFind::GetSearchLimits(nsRange* aSearchRange,
                                           nsRange* aStartPt, nsRange* aEndPt,
                                           Document* aDoc, Selection* aSel,
                                           bool aWrap) {
  NS_ENSURE_ARG_POINTER(aSel);

  // There is a selection.
  uint32_t count = aSel->RangeCount();
  if (count < 1) {
    return SetRangeAroundDocument(aSearchRange, aStartPt, aEndPt, aDoc);
  }

  // Need bodyContent, for the start/end of the document
  RefPtr<Element> bodyContent;
  nsresult rv = GetRootNode(aDoc, getter_AddRefs(bodyContent));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_ARG_POINTER(bodyContent);

  uint32_t childCount = bodyContent->GetChildCount();

  // There are four possible range endpoints we might use:
  // DocumentStart, SelectionStart, SelectionEnd, DocumentEnd.

  RefPtr<const nsRange> range;
  nsCOMPtr<nsINode> node;
  uint32_t offset;

  // Prevent the security checks in nsRange from getting into effect for the
  // purposes of determining the search range. These ranges will never be
  // exposed to content.
  mozilla::dom::AutoNoJSAPI nojsapi;

  // Forward, not wrapping: SelEnd to DocEnd
  if (!mFindBackwards && !aWrap) {
    // This isn't quite right, since the selection's ranges aren't
    // necessarily in order; but they usually will be.
    range = aSel->GetRangeAt(count - 1);
    if (!range) {
      return NS_ERROR_UNEXPECTED;
    }
    node = range->GetEndContainer();
    if (!node) {
      return NS_ERROR_UNEXPECTED;
    }
    offset = range->EndOffset();

    aSearchRange->SetStart(*node, offset, IgnoreErrors());
    aSearchRange->SetEnd(*bodyContent, childCount, IgnoreErrors());
    aStartPt->SetStart(*node, offset, IgnoreErrors());
    aStartPt->SetEnd(*node, offset, IgnoreErrors());
    aEndPt->SetStart(*bodyContent, childCount, IgnoreErrors());
    aEndPt->SetEnd(*bodyContent, childCount, IgnoreErrors());
  }
  // Backward, not wrapping: DocStart to SelStart
  else if (mFindBackwards && !aWrap) {
    range = aSel->GetRangeAt(0);
    if (!range) {
      return NS_ERROR_UNEXPECTED;
    }
    node = range->GetStartContainer();
    if (!node) {
      return NS_ERROR_UNEXPECTED;
    }
    offset = range->StartOffset();

    aSearchRange->SetStart(*bodyContent, 0, IgnoreErrors());
    aSearchRange->SetEnd(*bodyContent, childCount, IgnoreErrors());
    aStartPt->SetStart(*node, offset, IgnoreErrors());
    aStartPt->SetEnd(*node, offset, IgnoreErrors());
    aEndPt->SetStart(*bodyContent, 0, IgnoreErrors());
    aEndPt->SetEnd(*bodyContent, 0, IgnoreErrors());
  }
  // Forward, wrapping: DocStart to SelEnd
  else if (!mFindBackwards && aWrap) {
    range = aSel->GetRangeAt(count - 1);
    if (!range) {
      return NS_ERROR_UNEXPECTED;
    }
    node = range->GetEndContainer();
    if (!node) {
      return NS_ERROR_UNEXPECTED;
    }
    offset = range->EndOffset();

    aSearchRange->SetStart(*bodyContent, 0, IgnoreErrors());
    aSearchRange->SetEnd(*bodyContent, childCount, IgnoreErrors());
    aStartPt->SetStart(*bodyContent, 0, IgnoreErrors());
    aStartPt->SetEnd(*bodyContent, 0, IgnoreErrors());
    aEndPt->SetStart(*node, offset, IgnoreErrors());
    aEndPt->SetEnd(*node, offset, IgnoreErrors());
  }
  // Backward, wrapping: SelStart to DocEnd
  else if (mFindBackwards && aWrap) {
    range = aSel->GetRangeAt(0);
    if (!range) {
      return NS_ERROR_UNEXPECTED;
    }
    node = range->GetStartContainer();
    if (!node) {
      return NS_ERROR_UNEXPECTED;
    }
    offset = range->StartOffset();

    aSearchRange->SetStart(*bodyContent, 0, IgnoreErrors());
    aSearchRange->SetEnd(*bodyContent, childCount, IgnoreErrors());
    aStartPt->SetStart(*bodyContent, childCount, IgnoreErrors());
    aStartPt->SetEnd(*bodyContent, childCount, IgnoreErrors());
    aEndPt->SetStart(*node, offset, IgnoreErrors());
    aEndPt->SetEnd(*node, offset, IgnoreErrors());
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::GetSearchFrames(bool* aSearchFrames) {
  NS_ENSURE_ARG_POINTER(aSearchFrames);
  // this only returns true if we are searching both sub and parent frames.
  // There is ambiguity if the caller has previously set one, but not both of
  // these.
  *aSearchFrames = mSearchSubFrames && mSearchParentFrames;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::SetSearchFrames(bool aSearchFrames) {
  mSearchSubFrames = aSearchFrames;
  mSearchParentFrames = aSearchFrames;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::GetCurrentSearchFrame(
    mozIDOMWindowProxy** aCurrentSearchFrame) {
  NS_ENSURE_ARG_POINTER(aCurrentSearchFrame);
  nsCOMPtr<mozIDOMWindowProxy> searchFrame =
      do_QueryReferent(mCurrentSearchFrame);
  searchFrame.forget(aCurrentSearchFrame);
  return (*aCurrentSearchFrame) ? NS_OK : NS_ERROR_NOT_INITIALIZED;
}

NS_IMETHODIMP
nsWebBrowserFind::SetCurrentSearchFrame(
    mozIDOMWindowProxy* aCurrentSearchFrame) {
  // is it ever valid to set this to null?
  NS_ENSURE_ARG(aCurrentSearchFrame);
  mCurrentSearchFrame = do_GetWeakReference(aCurrentSearchFrame);
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::GetRootSearchFrame(mozIDOMWindowProxy** aRootSearchFrame) {
  NS_ENSURE_ARG_POINTER(aRootSearchFrame);
  nsCOMPtr<mozIDOMWindowProxy> searchFrame = do_QueryReferent(mRootSearchFrame);
  searchFrame.forget(aRootSearchFrame);
  return (*aRootSearchFrame) ? NS_OK : NS_ERROR_NOT_INITIALIZED;
}

NS_IMETHODIMP
nsWebBrowserFind::SetRootSearchFrame(mozIDOMWindowProxy* aRootSearchFrame) {
  // is it ever valid to set this to null?
  NS_ENSURE_ARG(aRootSearchFrame);
  mRootSearchFrame = do_GetWeakReference(aRootSearchFrame);
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::GetSearchSubframes(bool* aSearchSubframes) {
  NS_ENSURE_ARG_POINTER(aSearchSubframes);
  *aSearchSubframes = mSearchSubFrames;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::SetSearchSubframes(bool aSearchSubframes) {
  mSearchSubFrames = aSearchSubframes;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::GetSearchParentFrames(bool* aSearchParentFrames) {
  NS_ENSURE_ARG_POINTER(aSearchParentFrames);
  *aSearchParentFrames = mSearchParentFrames;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::SetSearchParentFrames(bool aSearchParentFrames) {
  mSearchParentFrames = aSearchParentFrames;
  return NS_OK;
}

/*
    This method handles finding in a single window (aka frame).

*/
nsresult nsWebBrowserFind::SearchInFrame(nsPIDOMWindowOuter* aWindow,
                                         bool aWrapping, bool* aDidFind) {
  NS_ENSURE_ARG(aWindow);
  NS_ENSURE_ARG_POINTER(aDidFind);

  *aDidFind = false;

  // Do security check, to ensure that the frame we're searching is
  // accessible from the frame where the Find is being run.

  // get a uri for the window
  RefPtr<Document> theDoc = aWindow->GetDoc();
  if (!theDoc) {
    return NS_ERROR_FAILURE;
  }

  if (!nsContentUtils::SubjectPrincipal()->Subsumes(theDoc->NodePrincipal())) {
    return NS_ERROR_DOM_PROP_ACCESS_DENIED;
  }

  nsresult rv;
  nsCOMPtr<nsIFind> find = do_CreateInstance(NS_FIND_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  (void)find->SetCaseSensitive(mMatchCase);
  (void)find->SetMatchDiacritics(mMatchDiacritics);
  (void)find->SetFindBackwards(mFindBackwards);

  (void)find->SetEntireWord(mEntireWord);

  // Now make sure the content (for actual finding) and frame (for
  // selection) models are up to date.
  theDoc->FlushPendingNotifications(FlushType::Frames);

  RefPtr<Selection> sel = GetFrameSelection(aWindow);
  NS_ENSURE_ARG_POINTER(sel);

  RefPtr<nsRange> searchRange = nsRange::Create(theDoc);
  RefPtr<nsRange> startPt = nsRange::Create(theDoc);
  RefPtr<nsRange> endPt = nsRange::Create(theDoc);

  RefPtr<nsRange> foundRange;

  rv = GetSearchLimits(searchRange, startPt, endPt, theDoc, sel, aWrapping);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = find->Find(mSearchString, searchRange, startPt, endPt,
                  getter_AddRefs(foundRange));

  if (NS_SUCCEEDED(rv) && foundRange) {
    *aDidFind = true;
    sel->RemoveAllRanges(IgnoreErrors());
    // Beware! This may flush notifications via synchronous
    // ScrollSelectionIntoView.
    SetSelectionAndScroll(aWindow, foundRange);
  }

  return rv;
}

// called when we start searching a frame that is not the initial focussed
// frame. Prepare the frame to be searched. we clear the selection, so that the
// search starts from the top of the frame.
nsresult nsWebBrowserFind::OnStartSearchFrame(nsPIDOMWindowOuter* aWindow) {
  return ClearFrameSelection(aWindow);
}

// called when we are done searching a frame and didn't find anything, and about
// about to start searching the next frame.
nsresult nsWebBrowserFind::OnEndSearchFrame(nsPIDOMWindowOuter* aWindow) {
  return NS_OK;
}

already_AddRefed<Selection> nsWebBrowserFind::GetFrameSelection(
    nsPIDOMWindowOuter* aWindow) {
  RefPtr<Document> doc = aWindow->GetDoc();
  if (!doc) {
    return nullptr;
  }

  PresShell* presShell = doc->GetPresShell();
  if (!presShell) {
    return nullptr;
  }

  // text input controls have their independent selection controllers that we
  // must use when they have focus.
  nsPresContext* presContext = presShell->GetPresContext();

  nsCOMPtr<nsPIDOMWindowOuter> focusedWindow;
  nsCOMPtr<nsIContent> focusedContent = nsFocusManager::GetFocusedDescendant(
      aWindow, nsFocusManager::eOnlyCurrentWindow,
      getter_AddRefs(focusedWindow));

  nsIFrame* frame =
      focusedContent ? focusedContent->GetPrimaryFrame() : nullptr;

  nsCOMPtr<nsISelectionController> selCon;
  RefPtr<Selection> sel;
  if (frame) {
    frame->GetSelectionController(presContext, getter_AddRefs(selCon));
    sel = selCon->GetSelection(nsISelectionController::SELECTION_NORMAL);
    if (sel && sel->RangeCount() > 0) {
      return sel.forget();
    }
  }

  sel = presShell->GetSelection(nsISelectionController::SELECTION_NORMAL);
  return sel.forget();
}

nsresult nsWebBrowserFind::ClearFrameSelection(nsPIDOMWindowOuter* aWindow) {
  NS_ENSURE_ARG(aWindow);
  RefPtr<Selection> selection = GetFrameSelection(aWindow);
  if (selection) {
    selection->RemoveAllRanges(IgnoreErrors());
  }

  return NS_OK;
}

nsresult nsWebBrowserFind::OnFind(nsPIDOMWindowOuter* aFoundWindow) {
  SetCurrentSearchFrame(aFoundWindow);

  // We don't want a selection to appear in two frames simultaneously
  nsCOMPtr<nsPIDOMWindowOuter> lastFocusedWindow =
      do_QueryReferent(mLastFocusedWindow);
  if (lastFocusedWindow && lastFocusedWindow != aFoundWindow) {
    ClearFrameSelection(lastFocusedWindow);
  }

  if (RefPtr<nsFocusManager> fm = nsFocusManager::GetFocusManager()) {
    // get the containing frame and focus it. For top-level windows, the right
    // window should already be focused.
    if (RefPtr<Element> frameElement =
            aFoundWindow->GetFrameElementInternal()) {
      fm->SetFocus(frameElement, 0);
    }

    mLastFocusedWindow = do_GetWeakReference(aFoundWindow);
  }

  return NS_OK;
}
