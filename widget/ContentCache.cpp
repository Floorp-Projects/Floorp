/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ContentCache.h"
#include "mozilla/TextEvents.h"
#include "nsIWidget.h"

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
{
}

void
ContentCache::Clear()
{
  mText.Truncate();
}

void
ContentCache::SetText(const nsAString& aText)
{
  mText = aText;
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
