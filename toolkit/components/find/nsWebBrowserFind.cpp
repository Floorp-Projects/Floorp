/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWebBrowserFind.h"

// Only need this for NS_FIND_CONTRACTID,
// else we could use nsIDOMRange.h and nsIFind.h.
#include "nsFind.h"

#include "nsIComponentManager.h"
#include "nsIScriptSecurityManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsPIDOMWindow.h"
#include "nsIURI.h"
#include "nsIDocShell.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIDocument.h"
#include "nsISelectionController.h"
#include "nsIFrame.h"
#include "nsITextControlFrame.h"
#include "nsReadableUtils.h"
#include "nsIContent.h"
#include "nsContentCID.h"
#include "nsIServiceManager.h"
#include "nsIObserverService.h"
#include "nsISupportsPrimitives.h"
#include "nsFind.h"
#include "nsError.h"
#include "nsFocusManager.h"
#include "nsRange.h"
#include "mozilla/Services.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Selection.h"
#include "nsISimpleEnumerator.h"
#include "nsContentUtils.h"
#include "nsGenericHTMLElement.h"

#if DEBUG
#include "nsIWebNavigation.h"
#include "nsString.h"
#endif

using mozilla::dom::Selection;
using mozilla::dom::Element;

nsWebBrowserFind::nsWebBrowserFind()
  : mFindBackwards(false)
  , mWrapFind(false)
  , mEntireWord(false)
  , mMatchCase(false)
  , mSearchSubFrames(true)
  , mSearchParentFrames(true)
{
}

nsWebBrowserFind::~nsWebBrowserFind()
{
}

NS_IMPL_ISUPPORTS(nsWebBrowserFind, nsIWebBrowserFind,
                  nsIWebBrowserFindInFrames)

NS_IMETHODIMP
nsWebBrowserFind::FindNext(bool* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = false;

  NS_ENSURE_TRUE(CanFindNext(), NS_ERROR_NOT_INITIALIZED);

  nsresult rv = NS_OK;
  nsCOMPtr<nsPIDOMWindowOuter> searchFrame = do_QueryReferent(mCurrentSearchFrame);
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
    return OnFind(searchFrame); // we are done
  }

  // if we are not searching other frames, return
  if (!mSearchSubFrames && !mSearchParentFrames) {
    return NS_OK;
  }

  nsIDocShell* rootDocShell = rootFrame->GetDocShell();
  if (!rootDocShell) {
    return NS_ERROR_FAILURE;
  }

  int32_t enumDirection = mFindBackwards ? nsIDocShell::ENUMERATE_BACKWARDS :
                                           nsIDocShell::ENUMERATE_FORWARDS;

  nsCOMPtr<nsISimpleEnumerator> docShellEnumerator;
  rv = rootDocShell->GetDocShellEnumerator(nsIDocShellTreeItem::typeAll,
                                           enumDirection,
                                           getter_AddRefs(docShellEnumerator));
  if (NS_FAILED(rv)) {
    return rv;
  }

  // remember where we started
  nsCOMPtr<nsIDocShellTreeItem> startingItem =
    do_QueryInterface(searchFrame->GetDocShell(), &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIDocShellTreeItem> curItem;

  // XXX We should avoid searching in frameset documents here.
  // We also need to honour mSearchSubFrames and mSearchParentFrames.
  bool hasMore, doFind = false;
  while (NS_SUCCEEDED(docShellEnumerator->HasMoreElements(&hasMore)) &&
         hasMore) {
    nsCOMPtr<nsISupports> curSupports;
    rv = docShellEnumerator->GetNext(getter_AddRefs(curSupports));
    if (NS_FAILED(rv)) {
      break;
    }
    curItem = do_QueryInterface(curSupports, &rv);
    if (NS_FAILED(rv)) {
      break;
    }

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
        return OnFind(searchFrame); // we are done
      }

      OnEndSearchFrame(searchFrame);
    }

    if (curItem.get() == startingItem.get()) {
      doFind = true; // start looking in frames after this one
    }
  }

  if (!mWrapFind) {
    // remember where we left off
    SetCurrentSearchFrame(searchFrame);
    return NS_OK;
  }

  // From here on, we're wrapping, first through the other frames, then finally
  // from the beginning of the starting frame back to the starting point.

  // because nsISimpleEnumerator is totally lame and isn't resettable, I have to
  // make a new one
  docShellEnumerator = nullptr;
  rv = rootDocShell->GetDocShellEnumerator(nsIDocShellTreeItem::typeAll,
                                           enumDirection,
                                           getter_AddRefs(docShellEnumerator));
  if (NS_FAILED(rv)) {
    return rv;
  }

  while (NS_SUCCEEDED(docShellEnumerator->HasMoreElements(&hasMore)) &&
         hasMore) {
    nsCOMPtr<nsISupports> curSupports;
    rv = docShellEnumerator->GetNext(getter_AddRefs(curSupports));
    if (NS_FAILED(rv)) {
      break;
    }
    curItem = do_QueryInterface(curSupports, &rv);
    if (NS_FAILED(rv)) {
      break;
    }

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
        return OnFind(searchFrame); // we are done
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
      return OnFind(searchFrame); // we are done
    }

    OnEndSearchFrame(searchFrame);
  }

  // remember where we left off
  SetCurrentSearchFrame(searchFrame);

  NS_ASSERTION(NS_SUCCEEDED(rv), "Something failed");
  return rv;
}

NS_IMETHODIMP
nsWebBrowserFind::GetSearchString(char16_t** aSearchString)
{
  NS_ENSURE_ARG_POINTER(aSearchString);
  *aSearchString = ToNewUnicode(mSearchString);
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::SetSearchString(const char16_t* aSearchString)
{
  mSearchString.Assign(aSearchString);
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::GetFindBackwards(bool* aFindBackwards)
{
  NS_ENSURE_ARG_POINTER(aFindBackwards);
  *aFindBackwards = mFindBackwards;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::SetFindBackwards(bool aFindBackwards)
{
  mFindBackwards = aFindBackwards;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::GetWrapFind(bool* aWrapFind)
{
  NS_ENSURE_ARG_POINTER(aWrapFind);
  *aWrapFind = mWrapFind;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::SetWrapFind(bool aWrapFind)
{
  mWrapFind = aWrapFind;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::GetEntireWord(bool* aEntireWord)
{
  NS_ENSURE_ARG_POINTER(aEntireWord);
  *aEntireWord = mEntireWord;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::SetEntireWord(bool aEntireWord)
{
  mEntireWord = aEntireWord;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::GetMatchCase(bool* aMatchCase)
{
  NS_ENSURE_ARG_POINTER(aMatchCase);
  *aMatchCase = mMatchCase;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::SetMatchCase(bool aMatchCase)
{
  mMatchCase = aMatchCase;
  return NS_OK;
}

static bool
IsInNativeAnonymousSubtree(nsIContent* aContent)
{
  while (aContent) {
    nsIContent* bindingParent = aContent->GetBindingParent();
    if (bindingParent == aContent) {
      return true;
    }

    aContent = bindingParent;
  }

  return false;
}

void
nsWebBrowserFind::SetSelectionAndScroll(nsPIDOMWindowOuter* aWindow,
                                        nsRange* aRange)
{
  nsCOMPtr<nsIDocument> doc = aWindow->GetDoc();
  if (!doc) {
    return;
  }

  nsIPresShell* presShell = doc->GetShell();
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
    if (!IsInNativeAnonymousSubtree(content)) {
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
    selection->AddRange(*aRange, IgnoreErrors());

    nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
    if (fm) {
      if (tcFrame) {
        RefPtr<Element> newFocusedElement =
          content->IsElement() ? content->AsElement() : nullptr;
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
nsresult
nsWebBrowserFind::GetRootNode(nsIDocument* aDoc, nsIDOMNode** aNode)
{
  NS_ENSURE_ARG_POINTER(aDoc);
  NS_ENSURE_ARG_POINTER(aNode);
  *aNode = 0;

  if (aDoc->IsHTMLOrXHTML()) {
    Element* body = aDoc->GetBody();
    NS_ENSURE_ARG_POINTER(body);
    NS_ADDREF(*aNode = body->AsDOMNode());
    return NS_OK;
  }

  // For non-HTML documents, the content root node will be the doc element.
  Element* root = aDoc->GetDocumentElement();
  NS_ENSURE_ARG_POINTER(root);
  NS_ADDREF(*aNode = root->AsDOMNode());
  return NS_OK;
}

nsresult
nsWebBrowserFind::SetRangeAroundDocument(nsRange* aSearchRange,
                                         nsRange* aStartPt,
                                         nsRange* aEndPt,
                                         nsIDocument* aDoc)
{
  nsCOMPtr<nsIDOMNode> bodyNode;
  nsresult rv = GetRootNode(aDoc, getter_AddRefs(bodyNode));
  nsCOMPtr<nsIContent> bodyContent(do_QueryInterface(bodyNode));
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
nsresult
nsWebBrowserFind::GetSearchLimits(nsRange* aSearchRange,
                                  nsRange* aStartPt, nsRange* aEndPt,
                                  nsIDocument* aDoc, Selection* aSel,
                                  bool aWrap)
{
  NS_ENSURE_ARG_POINTER(aSel);

  // There is a selection.
  uint32_t count = aSel->RangeCount();
  if (count < 1) {
    return SetRangeAroundDocument(aSearchRange, aStartPt, aEndPt, aDoc);
  }

  // Need bodyNode, for the start/end of the document
  nsCOMPtr<nsIDOMNode> bodyNode;
  nsresult rv = GetRootNode(aDoc, getter_AddRefs(bodyNode));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIContent> bodyContent(do_QueryInterface(bodyNode));
  NS_ENSURE_ARG_POINTER(bodyContent);

  uint32_t childCount = bodyContent->GetChildCount();

  // There are four possible range endpoints we might use:
  // DocumentStart, SelectionStart, SelectionEnd, DocumentEnd.

  RefPtr<nsRange> range;
  nsCOMPtr<nsINode> node;
  uint32_t offset;

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
nsWebBrowserFind::GetSearchFrames(bool* aSearchFrames)
{
  NS_ENSURE_ARG_POINTER(aSearchFrames);
  // this only returns true if we are searching both sub and parent frames.
  // There is ambiguity if the caller has previously set one, but not both of
  // these.
  *aSearchFrames = mSearchSubFrames && mSearchParentFrames;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::SetSearchFrames(bool aSearchFrames)
{
  mSearchSubFrames = aSearchFrames;
  mSearchParentFrames = aSearchFrames;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::GetCurrentSearchFrame(mozIDOMWindowProxy** aCurrentSearchFrame)
{
  NS_ENSURE_ARG_POINTER(aCurrentSearchFrame);
  nsCOMPtr<mozIDOMWindowProxy> searchFrame = do_QueryReferent(mCurrentSearchFrame);
  searchFrame.forget(aCurrentSearchFrame);
  return (*aCurrentSearchFrame) ? NS_OK : NS_ERROR_NOT_INITIALIZED;
}

NS_IMETHODIMP
nsWebBrowserFind::SetCurrentSearchFrame(mozIDOMWindowProxy* aCurrentSearchFrame)
{
  // is it ever valid to set this to null?
  NS_ENSURE_ARG(aCurrentSearchFrame);
  mCurrentSearchFrame = do_GetWeakReference(aCurrentSearchFrame);
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::GetRootSearchFrame(mozIDOMWindowProxy** aRootSearchFrame)
{
  NS_ENSURE_ARG_POINTER(aRootSearchFrame);
  nsCOMPtr<mozIDOMWindowProxy> searchFrame = do_QueryReferent(mRootSearchFrame);
  searchFrame.forget(aRootSearchFrame);
  return (*aRootSearchFrame) ? NS_OK : NS_ERROR_NOT_INITIALIZED;
}

NS_IMETHODIMP
nsWebBrowserFind::SetRootSearchFrame(mozIDOMWindowProxy* aRootSearchFrame)
{
  // is it ever valid to set this to null?
  NS_ENSURE_ARG(aRootSearchFrame);
  mRootSearchFrame = do_GetWeakReference(aRootSearchFrame);
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::GetSearchSubframes(bool* aSearchSubframes)
{
  NS_ENSURE_ARG_POINTER(aSearchSubframes);
  *aSearchSubframes = mSearchSubFrames;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::SetSearchSubframes(bool aSearchSubframes)
{
  mSearchSubFrames = aSearchSubframes;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::GetSearchParentFrames(bool* aSearchParentFrames)
{
  NS_ENSURE_ARG_POINTER(aSearchParentFrames);
  *aSearchParentFrames = mSearchParentFrames;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserFind::SetSearchParentFrames(bool aSearchParentFrames)
{
  mSearchParentFrames = aSearchParentFrames;
  return NS_OK;
}

/*
    This method handles finding in a single window (aka frame).

*/
nsresult
nsWebBrowserFind::SearchInFrame(nsPIDOMWindowOuter* aWindow, bool aWrapping,
                                bool* aDidFind)
{
  NS_ENSURE_ARG(aWindow);
  NS_ENSURE_ARG_POINTER(aDidFind);

  *aDidFind = false;

  // Do security check, to ensure that the frame we're searching is
  // accessible from the frame where the Find is being run.

  // get a uri for the window
  nsCOMPtr<nsIDocument> theDoc = aWindow->GetDoc();
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
  (void)find->SetFindBackwards(mFindBackwards);

  (void)find->SetEntireWord(mEntireWord);

  // Now make sure the content (for actual finding) and frame (for
  // selection) models are up to date.
  theDoc->FlushPendingNotifications(FlushType::Frames);

  RefPtr<Selection> sel = GetFrameSelection(aWindow);
  NS_ENSURE_ARG_POINTER(sel);

  RefPtr<nsRange> searchRange = new nsRange(theDoc);
  RefPtr<nsRange> startPt = new nsRange(theDoc);
  RefPtr<nsRange> endPt = new nsRange(theDoc);
  NS_ENSURE_ARG_POINTER(endPt);

  nsCOMPtr<nsIDOMRange> foundRange;

  // If !aWrapping, search from selection to end
  if (!aWrapping)
    rv = GetSearchLimits(searchRange, startPt, endPt, theDoc, sel, false);

  // If aWrapping, search the part of the starting frame
  // up to the point where we left off.
  else
    rv = GetSearchLimits(searchRange, startPt, endPt, theDoc, sel, true);

  NS_ENSURE_SUCCESS(rv, rv);

  rv = find->Find(mSearchString.get(), searchRange, startPt, endPt,
                  getter_AddRefs(foundRange));

  if (NS_SUCCEEDED(rv) && foundRange) {
    *aDidFind = true;
    sel->RemoveAllRanges(IgnoreErrors());
    // Beware! This may flush notifications via synchronous
    // ScrollSelectionIntoView.
    SetSelectionAndScroll(aWindow, static_cast<nsRange*>(foundRange.get()));
  }

  return rv;
}

// called when we start searching a frame that is not the initial focussed
// frame. Prepare the frame to be searched. we clear the selection, so that the
// search starts from the top of the frame.
nsresult
nsWebBrowserFind::OnStartSearchFrame(nsPIDOMWindowOuter* aWindow)
{
  return ClearFrameSelection(aWindow);
}

// called when we are done searching a frame and didn't find anything, and about
// about to start searching the next frame.
nsresult
nsWebBrowserFind::OnEndSearchFrame(nsPIDOMWindowOuter* aWindow)
{
  return NS_OK;
}

already_AddRefed<Selection>
nsWebBrowserFind::GetFrameSelection(nsPIDOMWindowOuter* aWindow)
{
  nsCOMPtr<nsIDocument> doc = aWindow->GetDoc();
  if (!doc) {
    return nullptr;
  }

  nsIPresShell* presShell = doc->GetShell();
  if (!presShell) {
    return nullptr;
  }

  // text input controls have their independent selection controllers that we
  // must use when they have focus.
  nsPresContext* presContext = presShell->GetPresContext();

  nsCOMPtr<nsPIDOMWindowOuter> focusedWindow;
  nsCOMPtr<nsIContent> focusedContent =
    nsFocusManager::GetFocusedDescendant(aWindow,
                                         nsFocusManager::eOnlyCurrentWindow,
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

  selCon = do_QueryInterface(presShell);
  sel = selCon->GetSelection(nsISelectionController::SELECTION_NORMAL);
  return sel.forget();
}

nsresult
nsWebBrowserFind::ClearFrameSelection(nsPIDOMWindowOuter* aWindow)
{
  NS_ENSURE_ARG(aWindow);
  RefPtr<Selection> selection = GetFrameSelection(aWindow);
  if (selection) {
    selection->RemoveAllRanges(IgnoreErrors());
  }

  return NS_OK;
}

nsresult
nsWebBrowserFind::OnFind(nsPIDOMWindowOuter* aFoundWindow)
{
  SetCurrentSearchFrame(aFoundWindow);

  // We don't want a selection to appear in two frames simultaneously
  nsCOMPtr<nsPIDOMWindowOuter> lastFocusedWindow =
    do_QueryReferent(mLastFocusedWindow);
  if (lastFocusedWindow && lastFocusedWindow != aFoundWindow) {
    ClearFrameSelection(lastFocusedWindow);
  }

  nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
  if (fm) {
    // get the containing frame and focus it. For top-level windows, the right
    // window should already be focused.
    RefPtr<Element> frameElement = aFoundWindow->GetFrameElementInternal();
    if (frameElement) {
      fm->SetFocus(frameElement, 0);
    }

    mLastFocusedWindow = do_GetWeakReference(aFoundWindow);
  }

  return NS_OK;
}
