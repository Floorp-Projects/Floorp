/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ContentCache_h
#define mozilla_ContentCache_h

#include <stdint.h>

#include "mozilla/Assertions.h"
#include "mozilla/EventForwards.h"
#include "mozilla/WritingModes.h"
#include "nsString.h"
#include "nsTArray.h"
#include "Units.h"

class nsIWidget;

namespace mozilla {

/**
 * ContentCache stores various information of the child content.  This hides
 * raw information but you can access more useful information with a lot of
 * methods.
 */

class ContentCache final
{
public:
  typedef InfallibleTArray<LayoutDeviceIntRect> RectArray;

  ContentCache();

  void Clear();

  /**
   * AssignContent() is called when TabParent receives ContentCache from
   * the content process.  This doesn't copy composition information because
   * it's managed by TabParent itself.
   */
  void AssignContent(const ContentCache& aOther)
  {
    mText = aOther.mText;
    mSelection = aOther.mSelection;
    mCaret = aOther.mCaret;
    mTextRectArray = aOther.mTextRectArray;
    mEditorRect = aOther.mEditorRect;
  }

  /**
   * HandleQueryContentEvent() sets content data to aEvent.mReply.
   *
   * For NS_QUERY_SELECTED_TEXT, fail if the cache doesn't contain the whole
   *  selected range. (This shouldn't happen because PuppetWidget should have
   *  already sent the whole selection.)
   *
   * For NS_QUERY_TEXT_CONTENT, fail only if the cache doesn't overlap with
   *  the queried range. Note the difference from above. We use
   *  this behavior because a normal NS_QUERY_TEXT_CONTENT event is allowed to
   *  have out-of-bounds offsets, so that widget can request content without
   *  knowing the exact length of text. It's up to widget to handle cases when
   *  the returned offset/length are different from the queried offset/length.
   *
   * For NS_QUERY_TEXT_RECT, fail if cached offset/length aren't equals to input.
   *   Cocoa widget always queries selected offset, so it works on it.
   *
   * For NS_QUERY_CARET_RECT, fail if cached offset isn't equals to input
   *
   * For NS_QUERY_EDITOR_RECT, always success
   */
  bool HandleQueryContentEvent(WidgetQueryContentEvent& aEvent,
                               nsIWidget* aWidget) const;

  /**
   * Cache*() retrieves the latest content information and store them.
   */
  bool CacheEditorRect(nsIWidget* aWidget);
  bool CacheSelection(nsIWidget* aWidget);
  bool CacheText(nsIWidget* aWidget);
  bool CacheTextRects(nsIWidget* aWidget);

  bool CacheAll(nsIWidget* aWidget);

  void SetText(const nsAString& aText);
  const nsString& Text() const { return mText; }
  uint32_t TextLength() const { return mText.Length(); }

  /**
   * OnCompositionEvent() should be called before sending composition string.
   * This returns true if the event should be sent.  Otherwise, false.
   */
  bool OnCompositionEvent(const WidgetCompositionEvent& aCompositionEvent);
  /**
   * RequestToCommitComposition() requests to commit or cancel composition to
   * the widget.  If it's handled synchronously, this returns the number of
   * composition events after that.
   *
   * @param aWidget     The widget to be requested to commit or cancel
   *                    the composition.
   * @param aCancel     When the caller tries to cancel the composition, true.
   *                    Otherwise, i.e., tries to commit the composition, false.
   * @param aLastString The last composition string before requesting to
   *                    commit or cancel composition.
   * @return            The count of composition events ignored after a call of
   *                    WillRequestToCommitOrCancelComposition().
   */
  uint32_t RequestToCommitComposition(nsIWidget* aWidget,
                                      bool aCancel,
                                      nsAString& aLastString);

  void SetSelection(uint32_t aCaretOffset, const WritingMode& aWritingMode)
  {
    SetSelection(aCaretOffset, aCaretOffset, aWritingMode);
  }
  void SetSelection(uint32_t aStartOffset,
                    uint32_t aLength,
                    bool aReversed,
                    const WritingMode& aWritingMode);
  void SetSelection(uint32_t aAnchorOffset,
                    uint32_t aFocusOffset,
                    const WritingMode& aWritingMode);
  bool SelectionCollapsed() const { return mSelection.Collapsed(); }
  bool SelectionReversed() const { return mSelection.Reversed(); }
  bool SelectionEndIsGraterThanTextLength() const
  {
    return SelectionEnd() > TextLength();
  }
  uint32_t SelectionAnchor() const { return mSelection.mAnchor; }
  uint32_t SelectionFocus() const { return mSelection.mFocus; }
  uint32_t SelectionStart() const { return mSelection.StartOffset(); }
  uint32_t SelectionEnd() const { return mSelection.EndOffset(); }
  uint32_t SelectionLength() const { return mSelection.Length(); }

  const WritingMode& SelectionWritingMode() const
  {
    return mSelection.mWritingMode;
  }

  bool UpdateTextRectArray(const RectArray& aTextRectArray)
  {
    return InitTextRectArray(mTextRectArray.mStart, aTextRectArray);
  }
  bool InitTextRectArray(uint32_t aOffset, const RectArray& aTextRectArray);
  bool GetTextRect(uint32_t aOffset,
                   LayoutDeviceIntRect& aTextRect) const;
  bool GetUnionTextRects(uint32_t aOffset,
                         uint32_t aLength,
                         LayoutDeviceIntRect& aUnionTextRect) const;

  bool UpdateCaretRect(const LayoutDeviceIntRect& aCaretRect)
  {
    return InitCaretRect(mCaret.mOffset, aCaretRect);
  }
  bool InitCaretRect(uint32_t aOffset, const LayoutDeviceIntRect& aCaretRect);
  uint32_t CaretOffset() const { return mCaret.mOffset; }
  bool GetCaretRect(uint32_t aOffset, LayoutDeviceIntRect& aCaretRect) const;

  void SetEditorRect(const LayoutDeviceIntRect& aEditorRect)
  {
    mEditorRect = aEditorRect;
  }
  const LayoutDeviceIntRect& GetEditorRect() const { return mEditorRect; }

private:
  // Whole text in the target
  nsString mText;
  // This is commit string which is caused by our request.
  nsString mCommitStringByRequest;
  // Start offset of the composition string.
  uint32_t mCompositionStart;
  // Count of composition events during requesting commit or cancel the
  // composition.
  uint32_t mCompositionEventsDuringRequest;

  struct Selection final
  {
    // Following values are offset in "flat text".
    uint32_t mAnchor;
    uint32_t mFocus;

    WritingMode mWritingMode;

    Selection()
      : mAnchor(0)
      , mFocus(0)
    {
    }

    void Clear()
    {
      mAnchor = mFocus = 0;
      mWritingMode = WritingMode();
    }

    bool Collapsed() const { return mFocus == mAnchor; }
    bool Reversed() const { return mFocus < mAnchor; }
    uint32_t StartOffset() const { return Reversed() ? mFocus : mAnchor; }
    uint32_t EndOffset() const { return Reversed() ? mAnchor : mFocus; }
    uint32_t Length() const
    {
      return Reversed() ? mAnchor - mFocus : mFocus - mAnchor;
    }
  } mSelection;

  struct Caret final
  {
    uint32_t mOffset;
    LayoutDeviceIntRect mRect;

    Caret()
      : mOffset(UINT32_MAX)
    {
    }

    void Clear()
    {
      mOffset = UINT32_MAX;
      mRect.SetEmpty();
    }

    uint32_t Offset() const
    {
      NS_WARN_IF(mOffset == UINT32_MAX);
      return mOffset;
    }
  } mCaret;

  struct TextRectArray final
  {
    uint32_t mStart;
    RectArray mRects;

    TextRectArray()
      : mStart(UINT32_MAX)
    {
    }

    void Clear()
    {
      mStart = UINT32_MAX;
      mRects.Clear();
    }

    uint32_t StartOffset() const
    {
      NS_WARN_IF(mStart == UINT32_MAX);
      return mStart;
    }
    uint32_t EndOffset() const
    {
      if (NS_WARN_IF(mStart == UINT32_MAX) ||
          NS_WARN_IF(static_cast<uint64_t>(mStart) + mRects.Length() >
                       UINT32_MAX)) {
        return UINT32_MAX;
      }
      return mStart + mRects.Length();
    }
    bool InRange(uint32_t aOffset) const
    {
      return mStart != UINT32_MAX &&
             StartOffset() <= aOffset && aOffset < EndOffset();
    }
    bool InRange(uint32_t aOffset, uint32_t aLength) const
    {
      if (NS_WARN_IF(static_cast<uint64_t>(aOffset) + aLength > UINT32_MAX)) {
        return false;
      }
      return InRange(aOffset) && aOffset + aLength <= EndOffset();
    }
    LayoutDeviceIntRect GetRect(uint32_t aOffset) const;
    LayoutDeviceIntRect GetUnionRect(uint32_t aOffset, uint32_t aLength) const;
  } mTextRectArray;

  LayoutDeviceIntRect mEditorRect;

  bool mIsComposing;
  bool mRequestedToCommitOrCancelComposition;

  friend struct IPC::ParamTraits<ContentCache>;
};

} // namespace mozilla

#endif // mozilla_ContentCache_h
