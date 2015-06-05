/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ContentCache.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/TextComposition.h"
#include "mozilla/TextEvents.h"
#include "nsIWidget.h"
#include "nsRefPtr.h"

namespace mozilla {

using namespace widget;

/*****************************************************************************
 * mozilla::ContentCache
 *****************************************************************************/

ContentCache::ContentCache()
  : mCompositionStart(UINT32_MAX)
  , mCompositionEventsDuringRequest(0)
  , mIsComposing(false)
  , mRequestedToCommitOrCancelComposition(false)
  , mIsChrome(XRE_GetProcessType() == GeckoProcessType_Default)
{
}

void
ContentCache::Clear()
{
  mText.Truncate();
  mSelection.Clear();
  mCaret.Clear();
  mTextRectArray.Clear();
  mEditorRect.SetEmpty();
}

bool
ContentCache::HandleQueryContentEvent(WidgetQueryContentEvent& aEvent,
                                      nsIWidget* aWidget) const
{
  MOZ_ASSERT(aWidget);

  aEvent.mSucceeded = false;
  aEvent.mWasAsync = false;
  aEvent.mReply.mFocusedWidget = aWidget;

  switch (aEvent.message) {
    case NS_QUERY_SELECTED_TEXT:
      aEvent.mReply.mOffset = mSelection.StartOffset();
      if (mSelection.Collapsed()) {
        aEvent.mReply.mString.Truncate(0);
      } else {
        if (NS_WARN_IF(SelectionEndIsGraterThanTextLength())) {
          return false;
        }
        aEvent.mReply.mString =
          Substring(mText, aEvent.mReply.mOffset, mSelection.Length());
      }
      aEvent.mReply.mReversed = mSelection.Reversed();
      aEvent.mReply.mHasSelection = true;
      aEvent.mReply.mWritingMode = mSelection.mWritingMode;
      break;
    case NS_QUERY_TEXT_CONTENT: {
      uint32_t inputOffset = aEvent.mInput.mOffset;
      uint32_t inputEndOffset =
        std::min(aEvent.mInput.EndOffset(), mText.Length());
      if (NS_WARN_IF(inputEndOffset < inputOffset)) {
        return false;
      }
      aEvent.mReply.mOffset = inputOffset;
      aEvent.mReply.mString =
        Substring(mText, inputOffset, inputEndOffset - inputOffset);
      break;
    }
    case NS_QUERY_TEXT_RECT:
      if (NS_WARN_IF(!GetUnionTextRects(aEvent.mInput.mOffset,
                                        aEvent.mInput.mLength,
                                        aEvent.mReply.mRect))) {
        // XXX We don't have cache for this request.
        return false;
      }
      if (aEvent.mInput.mOffset < mText.Length()) {
        aEvent.mReply.mString =
          Substring(mText, aEvent.mInput.mOffset,
                    mText.Length() >= aEvent.mInput.EndOffset() ?
                      aEvent.mInput.mLength : UINT32_MAX);
      } else {
        aEvent.mReply.mString.Truncate(0);
      }
      aEvent.mReply.mOffset = aEvent.mInput.mOffset;
      // XXX This may be wrong if storing range isn't in the selection range.
      aEvent.mReply.mWritingMode = mSelection.mWritingMode;
      break;
    case NS_QUERY_CARET_RECT:
      if (NS_WARN_IF(!GetCaretRect(aEvent.mInput.mOffset,
                                   aEvent.mReply.mRect))) {
        // XXX If the input offset is in the range of cached text rects,
        //     we can guess the caret rect.
        return false;
      }
      aEvent.mReply.mOffset = aEvent.mInput.mOffset;
      break;
    case NS_QUERY_EDITOR_RECT:
      aEvent.mReply.mRect = mEditorRect;
      break;
  }
  aEvent.mSucceeded = true;
  return true;
}

bool
ContentCache::CacheAll(nsIWidget* aWidget)
{
  // CacheAll() must be called in content process.
  if (NS_WARN_IF(mIsChrome)) {
    return false;
  }

  if (NS_WARN_IF(!CacheText(aWidget)) ||
      NS_WARN_IF(!CacheSelection(aWidget)) ||
      NS_WARN_IF(!CacheTextRects(aWidget)) ||
      NS_WARN_IF(!CacheEditorRect(aWidget))) {
    return false;
  }
  return true;
}

bool
ContentCache::CacheSelection(nsIWidget* aWidget)
{
  // CacheSelection() must be called in content process.
  if (NS_WARN_IF(mIsChrome)) {
    return false;
  }

  mCaret.Clear();
  mSelection.Clear();

  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetQueryContentEvent selection(true, NS_QUERY_SELECTED_TEXT, aWidget);
  aWidget->DispatchEvent(&selection, status);
  if (NS_WARN_IF(!selection.mSucceeded)) {
    return false;
  }
  if (selection.mReply.mReversed) {
    mSelection.mAnchor =
      selection.mReply.mOffset + selection.mReply.mString.Length();
    mSelection.mFocus = selection.mReply.mOffset;
  } else {
    mSelection.mAnchor = selection.mReply.mOffset;
    mSelection.mFocus =
      selection.mReply.mOffset + selection.mReply.mString.Length();
  }
  mSelection.mWritingMode = selection.GetWritingMode();

  nsRefPtr<TextComposition> textComposition =
    IMEStateManager::GetTextCompositionFor(aWidget);
  if (textComposition) {
    mCaret.mOffset = textComposition->OffsetOfTargetClause();
  } else {
    mCaret.mOffset = selection.mReply.mOffset;
  }

  status = nsEventStatus_eIgnore;
  WidgetQueryContentEvent caretRect(true, NS_QUERY_CARET_RECT, aWidget);
  caretRect.InitForQueryCaretRect(mCaret.mOffset);
  aWidget->DispatchEvent(&caretRect, status);
  if (NS_WARN_IF(!caretRect.mSucceeded)) {
    mCaret.Clear();
    return false;
  }
  mCaret.mRect = caretRect.mReply.mRect;
  return true;
}

bool
ContentCache::CacheEditorRect(nsIWidget* aWidget)
{
  // CacheEditorRect() must be called in content process.
  if (NS_WARN_IF(mIsChrome)) {
    return false;
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetQueryContentEvent editorRectEvent(true, NS_QUERY_EDITOR_RECT, aWidget);
  aWidget->DispatchEvent(&editorRectEvent, status);
  if (NS_WARN_IF(!editorRectEvent.mSucceeded)) {
    return false;
  }
  mEditorRect = editorRectEvent.mReply.mRect;
  return true;
}

bool
ContentCache::CacheText(nsIWidget* aWidget)
{
  // CacheText() must be called in content process.
  if (NS_WARN_IF(mIsChrome)) {
    return false;
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetQueryContentEvent queryText(true, NS_QUERY_TEXT_CONTENT, aWidget);
  queryText.InitForQueryTextContent(0, UINT32_MAX);
  aWidget->DispatchEvent(&queryText, status);
  if (NS_WARN_IF(!queryText.mSucceeded)) {
    SetText(EmptyString());
    return false;
  }
  SetText(queryText.mReply.mString);
  return true;
}

bool
ContentCache::CacheTextRects(nsIWidget* aWidget)
{
  // CacheTextRects() must be called in content process.
  if (NS_WARN_IF(mIsChrome)) {
    return false;
  }

  mTextRectArray.Clear();

  nsRefPtr<TextComposition> textComposition =
    IMEStateManager::GetTextCompositionFor(aWidget);
  if (NS_WARN_IF(!textComposition)) {
    return true;
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  mTextRectArray.mRects.SetCapacity(textComposition->String().Length());
  mTextRectArray.mStart = textComposition->NativeOffsetOfStartComposition();
  uint32_t endOffset =
    mTextRectArray.mStart + textComposition->String().Length();
  for (uint32_t i = mTextRectArray.mStart; i < endOffset; i++) {
    WidgetQueryContentEvent textRect(true, NS_QUERY_TEXT_RECT, aWidget);
    textRect.InitForQueryTextRect(i, 1);
    aWidget->DispatchEvent(&textRect, status);
    if (NS_WARN_IF(!textRect.mSucceeded)) {
      mTextRectArray.Clear();
      return false;
    }
    mTextRectArray.mRects.AppendElement(textRect.mReply.mRect);
  }
  return true;
}

void
ContentCache::SetText(const nsAString& aText)
{
  mText = aText;
}

void
ContentCache::SetSelection(uint32_t aStartOffset,
                           uint32_t aLength,
                           bool aReversed,
                           const WritingMode& aWritingMode)
{
  if (!aReversed) {
    mSelection.mAnchor = aStartOffset;
    mSelection.mFocus = aStartOffset + aLength;
  } else {
    mSelection.mAnchor = aStartOffset + aLength;
    mSelection.mFocus = aStartOffset;
  }
  mSelection.mWritingMode = aWritingMode;
}

void
ContentCache::SetSelection(uint32_t aAnchorOffset,
                           uint32_t aFocusOffset,
                           const WritingMode& aWritingMode)
{
  mSelection.mAnchor = aAnchorOffset;
  mSelection.mFocus = aFocusOffset;
  mSelection.mWritingMode = aWritingMode;
}

bool
ContentCache::InitTextRectArray(uint32_t aOffset,
                                const RectArray& aTextRectArray)
{
  if (NS_WARN_IF(aOffset >= TextLength()) ||
      NS_WARN_IF(aOffset + aTextRectArray.Length() > TextLength())) {
    return false;
  }
  mTextRectArray.mStart = aOffset;
  mTextRectArray.mRects = aTextRectArray;
  return true;
}

bool
ContentCache::GetTextRect(uint32_t aOffset,
                          LayoutDeviceIntRect& aTextRect) const
{
  if (NS_WARN_IF(!mTextRectArray.InRange(aOffset))) {
    aTextRect.SetEmpty();
    return false;
  }
  aTextRect = mTextRectArray.GetRect(aOffset);
  return true;
}

bool
ContentCache::GetUnionTextRects(uint32_t aOffset,
                                uint32_t aLength,
                                LayoutDeviceIntRect& aUnionTextRect) const
{
  if (NS_WARN_IF(!mTextRectArray.InRange(aOffset, aLength))) {
    aUnionTextRect.SetEmpty();
    return false;
  }
  aUnionTextRect = mTextRectArray.GetUnionRect(aOffset, aLength);
  return true;
}

bool
ContentCache::InitCaretRect(uint32_t aOffset,
                            const LayoutDeviceIntRect& aCaretRect)
{
  if (NS_WARN_IF(aOffset > TextLength())) {
    return false;
  }
  mCaret.mOffset = aOffset;
  mCaret.mRect = aCaretRect;
  return true;
}

bool
ContentCache::GetCaretRect(uint32_t aOffset,
                           LayoutDeviceIntRect& aCaretRect) const
{
  if (mCaret.mOffset != aOffset) {
    aCaretRect.SetEmpty();
    return false;
  }
  aCaretRect = mCaret.mRect;
  return true;
}

bool
ContentCache::OnCompositionEvent(const WidgetCompositionEvent& aEvent)
{
  if (!aEvent.CausesDOMTextEvent()) {
    MOZ_ASSERT(aEvent.message == NS_COMPOSITION_START);
    mIsComposing = !aEvent.CausesDOMCompositionEndEvent();
    mCompositionStart = SelectionStart();
    // XXX What's this case??
    if (mRequestedToCommitOrCancelComposition) {
      mCommitStringByRequest = aEvent.mData;
      mCompositionEventsDuringRequest++;
      return false;
    }
    return true;
  }

  // XXX Why do we ignore following composition events here?
  //     TextComposition must handle following events correctly!

  // During REQUEST_TO_COMMIT_COMPOSITION or REQUEST_TO_CANCEL_COMPOSITION,
  // widget usually sends a NS_COMPOSITION_CHANGE event to finalize or
  // clear the composition, respectively.
  // Because the event will not reach content in time, we intercept it
  // here and pass the text as the DidRequestToCommitOrCancelComposition()
  // return value.
  if (mRequestedToCommitOrCancelComposition) {
    mCommitStringByRequest = aEvent.mData;
    mCompositionEventsDuringRequest++;
    return false;
  }

  // We must be able to simulate the selection because
  // we might not receive selection updates in time
  if (!mIsComposing) {
    mCompositionStart = SelectionStart();
  }
  // XXX This causes different behavior from non-e10s mode.
  //     Selection range should represent caret position in the composition
  //     string but this means selection range is all of the composition string.
  SetSelection(mCompositionStart + aEvent.mData.Length(),
               mSelection.mWritingMode);
  mIsComposing = !aEvent.CausesDOMCompositionEndEvent();
  return true;
}

uint32_t
ContentCache::RequestToCommitComposition(nsIWidget* aWidget,
                                         bool aCancel,
                                         nsAString& aLastString)
{
  mRequestedToCommitOrCancelComposition = true;
  mCompositionEventsDuringRequest = 0;

  aWidget->NotifyIME(IMENotification(aCancel ? REQUEST_TO_CANCEL_COMPOSITION :
                                               REQUEST_TO_COMMIT_COMPOSITION));

  mRequestedToCommitOrCancelComposition = false;
  aLastString = mCommitStringByRequest;
  mCommitStringByRequest.Truncate(0);
  return mCompositionEventsDuringRequest;
}

/*****************************************************************************
 * mozilla::ContentCache::TextRectArray
 *****************************************************************************/

LayoutDeviceIntRect
ContentCache::TextRectArray::GetRect(uint32_t aOffset) const
{
  LayoutDeviceIntRect rect;
  if (InRange(aOffset)) {
    rect = mRects[aOffset - mStart];
  }
  return rect;
}

LayoutDeviceIntRect
ContentCache::TextRectArray::GetUnionRect(uint32_t aOffset,
                                          uint32_t aLength) const
{
  LayoutDeviceIntRect rect;
  if (!InRange(aOffset, aLength)) {
    return rect;
  }
  for (uint32_t i = 0; i < aLength; i++) {
    rect = rect.Union(mRects[aOffset - mStart + i]);
  }
  return rect;
}

} // namespace mozilla
