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
#include "mozilla/CheckedInt.h"
#include "mozilla/EventForwards.h"
#include "mozilla/WritingModes.h"
#include "nsIWidget.h"
#include "nsString.h"
#include "nsTArray.h"
#include "Units.h"

namespace mozilla {

class ContentCacheInParent;

/**
 * ContentCache stores various information of the child content.
 * This class has members which are necessary both in parent process and
 * content process.
 */

class ContentCache
{
public:
  typedef InfallibleTArray<LayoutDeviceIntRect> RectArray;
  typedef widget::IMENotification IMENotification;

  ContentCache();

protected:
  // Whole text in the target
  nsString mText;

  struct Selection final
  {
    // Following values are offset in "flat text".
    uint32_t mAnchor;
    uint32_t mFocus;

    WritingMode mWritingMode;

    // Character rects at next character of mAnchor and mFocus.
    LayoutDeviceIntRect mAnchorCharRect;
    LayoutDeviceIntRect mFocusCharRect;

    // Whole rect of selected text. This is empty if the selection is collapsed.
    LayoutDeviceIntRect mRect;

    Selection()
      : mAnchor(UINT32_MAX)
      , mFocus(UINT32_MAX)
    {
    }

    void Clear()
    {
      mAnchor = mFocus = UINT32_MAX;
      mWritingMode = WritingMode();
      mAnchorCharRect.SetEmpty();
      mFocusCharRect.SetEmpty();
      mRect.SetEmpty();
    }

    bool IsValid() const
    {
      return mAnchor != UINT32_MAX && mFocus != UINT32_MAX;
    }
    bool Collapsed() const
    {
      NS_ASSERTION(IsValid(),
                   "The caller should check if the selection is valid");
      return mFocus == mAnchor;
    }
    bool Reversed() const
    {
      NS_ASSERTION(IsValid(),
                   "The caller should check if the selection is valid");
      return mFocus < mAnchor;
    }
    uint32_t StartOffset() const
    {
      NS_ASSERTION(IsValid(),
                   "The caller should check if the selection is valid");
      return Reversed() ? mFocus : mAnchor;
    }
    uint32_t EndOffset() const
    {
      NS_ASSERTION(IsValid(),
                   "The caller should check if the selection is valid");
      return Reversed() ? mAnchor : mFocus;
    }
    uint32_t Length() const
    {
      NS_ASSERTION(IsValid(),
                   "The caller should check if the selection is valid");
      return Reversed() ? mAnchor - mFocus : mFocus - mAnchor;
    }
  } mSelection;

  bool IsSelectionValid() const
  {
    return mSelection.IsValid() && mSelection.EndOffset() <= mText.Length();
  }

  // Stores first char rect because Yosemite's Japanese IME sometimes tries
  // to query it.  If there is no text, this is caret rect.
  LayoutDeviceIntRect mFirstCharRect;

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

    bool IsValid() const { return mOffset != UINT32_MAX; }

    uint32_t Offset() const
    {
      NS_ASSERTION(IsValid(),
                   "The caller should check if the caret is valid");
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

    bool IsValid() const
    {
      if (mStart == UINT32_MAX) {
        return false;
      }
      CheckedInt<uint32_t> endOffset =
        CheckedInt<uint32_t>(mStart) + mRects.Length();
      return endOffset.isValid();
    }
    uint32_t StartOffset() const
    {
      NS_ASSERTION(IsValid(),
                   "The caller should check if the caret is valid");
      return mStart;
    }
    uint32_t EndOffset() const
    {
      NS_ASSERTION(IsValid(),
                   "The caller should check if the caret is valid");
      if (!IsValid()) {
        return UINT32_MAX;
      }
      return mStart + mRects.Length();
    }
    bool InRange(uint32_t aOffset) const
    {
      return IsValid() &&
             StartOffset() <= aOffset && aOffset < EndOffset();
    }
    bool InRange(uint32_t aOffset, uint32_t aLength) const
    {
      CheckedInt<uint32_t> endOffset =
        CheckedInt<uint32_t>(aOffset) + aLength;
      if (NS_WARN_IF(!endOffset.isValid())) {
        return false;
      }
      return InRange(aOffset) && aOffset + aLength <= EndOffset();
    }
    bool IsOverlappingWith(uint32_t aOffset, uint32_t aLength) const
    {
      if (!IsValid() || aOffset == UINT32_MAX) {
        return false;
      }
      CheckedInt<uint32_t> endOffset =
        CheckedInt<uint32_t>(aOffset) + aLength;
      if (NS_WARN_IF(!endOffset.isValid())) {
        return false;
      }
      return aOffset <= EndOffset() && endOffset.value() >= mStart;
    }
    LayoutDeviceIntRect GetRect(uint32_t aOffset) const;
    LayoutDeviceIntRect GetUnionRect(uint32_t aOffset, uint32_t aLength) const;
    LayoutDeviceIntRect GetUnionRectAsFarAsPossible(uint32_t aOffset,
                                                    uint32_t aLength) const;
  } mTextRectArray;

  LayoutDeviceIntRect mEditorRect;

  friend class ContentCacheInParent;
  friend struct IPC::ParamTraits<ContentCache>;
};

class ContentCacheInChild final : public ContentCache
{
public:
  ContentCacheInChild();

  /**
   * When IME loses focus, this should be called and making this forget the
   * content for reducing footprint.
   */
  void Clear();

  /**
   * Cache*() retrieves the latest content information and store them.
   * Be aware, CacheSelection() calls CacheTextRects(), and also CacheText()
   * calls CacheSelection().  So, related data is also retrieved automatically.
   */
  bool CacheEditorRect(nsIWidget* aWidget,
                       const IMENotification* aNotification = nullptr);
  bool CacheSelection(nsIWidget* aWidget,
                      const IMENotification* aNotification = nullptr);
  bool CacheText(nsIWidget* aWidget,
                 const IMENotification* aNotification = nullptr);

  bool CacheAll(nsIWidget* aWidget,
                const IMENotification* aNotification = nullptr);

  /**
   * SetSelection() modifies selection with specified raw data. And also this
   * tries to retrieve text rects too.
   */
  void SetSelection(nsIWidget* aWidget,
                    uint32_t aStartOffset,
                    uint32_t aLength,
                    bool aReversed,
                    const WritingMode& aWritingMode);

private:
  bool QueryCharRect(nsIWidget* aWidget,
                     uint32_t aOffset,
                     LayoutDeviceIntRect& aCharRect) const;
  bool CacheCaret(nsIWidget* aWidget,
                  const IMENotification* aNotification = nullptr);
  bool CacheTextRects(nsIWidget* aWidget,
                      const IMENotification* aNotification = nullptr);
};

class ContentCacheInParent final : public ContentCache
{
public:
  ContentCacheInParent();

  /**
   * AssignContent() is called when TabParent receives ContentCache from
   * the content process.  This doesn't copy composition information because
   * it's managed by TabParent itself.
   */
  void AssignContent(const ContentCache& aOther,
                     const IMENotification* aNotification = nullptr);

  /**
   * HandleQueryContentEvent() sets content data to aEvent.mReply.
   *
   * For eQuerySelectedText, fail if the cache doesn't contain the whole
   *  selected range. (This shouldn't happen because PuppetWidget should have
   *  already sent the whole selection.)
   *
   * For eQueryTextContent, fail only if the cache doesn't overlap with
   *  the queried range. Note the difference from above. We use
   *  this behavior because a normal eQueryTextContent event is allowed to
   *  have out-of-bounds offsets, so that widget can request content without
   *  knowing the exact length of text. It's up to widget to handle cases when
   *  the returned offset/length are different from the queried offset/length.
   *
   * For NS_QUERY_TEXT_RECT, fail if cached offset/length aren't equals to input.
   *   Cocoa widget always queries selected offset, so it works on it.
   *
   * For eQueryCaretRect, fail if cached offset isn't equals to input
   *
   * For eQueryEditorRect, always success
   */
  bool HandleQueryContentEvent(WidgetQueryContentEvent& aEvent,
                               nsIWidget* aWidget) const;

  /**
   * OnCompositionEvent() should be called before sending composition string.
   * This returns true if the event should be sent.  Otherwise, false.
   */
  bool OnCompositionEvent(const WidgetCompositionEvent& aCompositionEvent);

  /**
   * OnSelectionEvent() should be called before sending selection event.
   */
  void OnSelectionEvent(const WidgetSelectionEvent& aSelectionEvent);

  /**
   * OnEventNeedingAckHandled() should be called after the child process
   * handles a sent event which needs acknowledging.
   *
   * WARNING: This may send notifications to IME.  That might cause destroying
   *          TabParent or aWidget.  Therefore, the caller must not destroy
   *          this instance during a call of this method.
   */
  void OnEventNeedingAckHandled(nsIWidget* aWidget, EventMessage aMessage);

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

  /**
   * MaybeNotifyIME() may notify IME of the notification.  If child process
   * hasn't been handled all sending events yet, this stores the notification
   * and flush it later.
   */
  void MaybeNotifyIME(nsIWidget* aWidget,
                      const IMENotification& aNotification);

private:
  IMENotification mPendingSelectionChange;
  IMENotification mPendingTextChange;
  IMENotification mPendingLayoutChange;
  IMENotification mPendingCompositionUpdate;

  // This is commit string which is caused by our request.
  nsString mCommitStringByRequest;
  // Start offset of the composition string.
  uint32_t mCompositionStart;
  // Count of composition events during requesting commit or cancel the
  // composition.
  uint32_t mCompositionEventsDuringRequest;
  // mPendingEventsNeedingAck is increased before sending a composition event or
  // a selection event and decreased after they are received in the child
  // process.
  uint32_t mPendingEventsNeedingAck;

  bool mIsComposing;
  bool mRequestedToCommitOrCancelComposition;

  bool GetCaretRect(uint32_t aOffset, LayoutDeviceIntRect& aCaretRect) const;
  bool GetTextRect(uint32_t aOffset,
                   LayoutDeviceIntRect& aTextRect) const;
  bool GetUnionTextRects(uint32_t aOffset,
                         uint32_t aLength,
                         LayoutDeviceIntRect& aUnionTextRect) const;

  void FlushPendingNotifications(nsIWidget* aWidget);
};

} // namespace mozilla

#endif // mozilla_ContentCache_h
