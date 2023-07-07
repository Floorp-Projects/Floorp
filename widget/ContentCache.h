/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ContentCache_h
#define mozilla_ContentCache_h

#include <stdint.h>

#include "mozilla/widget/IMEData.h"
#include "mozilla/ipc/IPCForwards.h"
#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/EventForwards.h"
#include "mozilla/Maybe.h"
#include "mozilla/ToString.h"
#include "mozilla/WritingModes.h"
#include "nsString.h"
#include "nsTArray.h"
#include "Units.h"

class nsIWidget;

namespace mozilla {

class ContentCacheInParent;

namespace dom {
class BrowserParent;
}  // namespace dom

/**
 * ContentCache stores various information of the child content.
 * This class has members which are necessary both in parent process and
 * content process.
 */

class ContentCache {
 public:
  using RectArray = CopyableTArray<LayoutDeviceIntRect>;
  using IMENotification = widget::IMENotification;

  ContentCache() = default;

  [[nodiscard]] bool IsValid() const;

 protected:
  void AssertIfInvalid() const;

  // Whole text in the target
  Maybe<nsString> mText;

  // Start offset of the composition string.
  Maybe<uint32_t> mCompositionStart;

  enum { ePrevCharRect = 1, eNextCharRect = 0 };

  struct Selection final {
    // Following values are offset in "flat text".
    uint32_t mAnchor;
    uint32_t mFocus;

    WritingMode mWritingMode;

    bool mHasRange;

    // Character rects at previous and next character of mAnchor and mFocus.
    // The reason why ContentCache needs to store each previous character of
    // them is IME may query character rect of the last character of a line
    // when caret is at the end of the line.
    // Note that use ePrevCharRect and eNextCharRect for accessing each item.
    LayoutDeviceIntRect mAnchorCharRects[2];
    LayoutDeviceIntRect mFocusCharRects[2];

    // Whole rect of selected text. This is empty if the selection is collapsed.
    LayoutDeviceIntRect mRect;

    Selection() : mAnchor(UINT32_MAX), mFocus(UINT32_MAX), mHasRange(false) {
      ClearRects();
    };

    explicit Selection(
        const IMENotification::SelectionChangeDataBase& aSelectionChangeData)
        : mAnchor(UINT32_MAX),
          mFocus(UINT32_MAX),
          mWritingMode(aSelectionChangeData.GetWritingMode()),
          mHasRange(aSelectionChangeData.HasRange()) {
      if (mHasRange) {
        mAnchor = aSelectionChangeData.AnchorOffset();
        mFocus = aSelectionChangeData.FocusOffset();
      }
    }

    [[nodiscard]] bool IsValidIn(const nsAString& aText) const {
      return !mHasRange ||
             (mAnchor <= aText.Length() && mFocus <= aText.Length());
    }

    explicit Selection(const WidgetQueryContentEvent& aQuerySelectedTextEvent);

    void ClearRects() {
      for (auto& rect : mAnchorCharRects) {
        rect.SetEmpty();
      }
      for (auto& rect : mFocusCharRects) {
        rect.SetEmpty();
      }
      mRect.SetEmpty();
    }
    bool HasRects() const {
      for (const auto& rect : mAnchorCharRects) {
        if (!rect.IsEmpty()) {
          return true;
        }
      }
      for (const auto& rect : mFocusCharRects) {
        if (!rect.IsEmpty()) {
          return true;
        }
      }
      return !mRect.IsEmpty();
    }

    bool IsCollapsed() const { return !mHasRange || mFocus == mAnchor; }
    bool Reversed() const {
      MOZ_ASSERT(mHasRange);
      return mFocus < mAnchor;
    }
    uint32_t StartOffset() const {
      MOZ_ASSERT(mHasRange);
      return Reversed() ? mFocus : mAnchor;
    }
    uint32_t EndOffset() const {
      MOZ_ASSERT(mHasRange);
      return Reversed() ? mAnchor : mFocus;
    }
    uint32_t Length() const {
      MOZ_ASSERT(mHasRange);
      return Reversed() ? mAnchor - mFocus : mFocus - mAnchor;
    }
    LayoutDeviceIntRect StartCharRect() const {
      return Reversed() ? mFocusCharRects[eNextCharRect]
                        : mAnchorCharRects[eNextCharRect];
    }
    LayoutDeviceIntRect EndCharRect() const {
      return Reversed() ? mAnchorCharRects[eNextCharRect]
                        : mFocusCharRects[eNextCharRect];
    }

    friend std::ostream& operator<<(std::ostream& aStream,
                                    const Selection& aSelection) {
      aStream << "{ ";
      if (!aSelection.mHasRange) {
        aStream << "HasRange()=false";
      } else {
        aStream << "mAnchor=" << aSelection.mAnchor
                << ", mFocus=" << aSelection.mFocus << ", mWritingMode="
                << ToString(aSelection.mWritingMode).c_str();
      }
      if (aSelection.HasRects()) {
        if (aSelection.mAnchor > 0) {
          aStream << ", mAnchorCharRects[ePrevCharRect]="
                  << aSelection.mAnchorCharRects[ContentCache::ePrevCharRect];
        }
        aStream << ", mAnchorCharRects[eNextCharRect]="
                << aSelection.mAnchorCharRects[ContentCache::eNextCharRect];
        if (aSelection.mFocus > 0) {
          aStream << ", mFocusCharRects[ePrevCharRect]="
                  << aSelection.mFocusCharRects[ContentCache::ePrevCharRect];
        }
        aStream << ", mFocusCharRects[eNextCharRect]="
                << aSelection.mFocusCharRects[ContentCache::eNextCharRect]
                << ", mRect=" << aSelection.mRect;
      }
      if (aSelection.mHasRange) {
        aStream << ", Reversed()=" << (aSelection.Reversed() ? "true" : "false")
                << ", StartOffset()=" << aSelection.StartOffset()
                << ", EndOffset()=" << aSelection.EndOffset()
                << ", IsCollapsed()="
                << (aSelection.IsCollapsed() ? "true" : "false")
                << ", Length()=" << aSelection.Length();
      }
      aStream << " }";
      return aStream;
    }
  };
  Maybe<Selection> mSelection;

  // Stores first char rect because Yosemite's Japanese IME sometimes tries
  // to query it.  If there is no text, this is caret rect.
  LayoutDeviceIntRect mFirstCharRect;

  struct Caret final {
    uint32_t mOffset = 0u;
    LayoutDeviceIntRect mRect;

    explicit Caret(uint32_t aOffset, LayoutDeviceIntRect aCaretRect)
        : mOffset(aOffset), mRect(aCaretRect) {}

    uint32_t Offset() const { return mOffset; }
    bool HasRect() const { return !mRect.IsEmpty(); }

    [[nodiscard]] bool IsValidIn(const nsAString& aText) const {
      return mOffset <= aText.Length();
    }

    friend std::ostream& operator<<(std::ostream& aStream,
                                    const Caret& aCaret) {
      aStream << "{ mOffset=" << aCaret.mOffset;
      if (aCaret.HasRect()) {
        aStream << ", mRect=" << aCaret.mRect;
      }
      return aStream << " }";
    }

   private:
    // For ParamTraits<Caret>
    Caret() = default;

    friend struct IPC::ParamTraits<ContentCache::Caret>;
    ALLOW_DEPRECATED_READPARAM
  };
  Maybe<Caret> mCaret;

  struct TextRectArray final {
    uint32_t mStart = 0u;
    RectArray mRects;

    explicit TextRectArray(uint32_t aStartOffset) : mStart(aStartOffset) {}

    bool HasRects() const { return Length() > 0; }
    uint32_t StartOffset() const { return mStart; }
    uint32_t EndOffset() const {
      CheckedInt<uint32_t> endOffset =
          CheckedInt<uint32_t>(mStart) + mRects.Length();
      return endOffset.isValid() ? endOffset.value() : UINT32_MAX;
    }
    uint32_t Length() const { return EndOffset() - mStart; }
    bool IsOffsetInRange(uint32_t aOffset) const {
      return StartOffset() <= aOffset && aOffset < EndOffset();
    }
    bool IsRangeCompletelyInRange(uint32_t aOffset, uint32_t aLength) const {
      CheckedInt<uint32_t> endOffset = CheckedInt<uint32_t>(aOffset) + aLength;
      if (NS_WARN_IF(!endOffset.isValid())) {
        return false;
      }
      return IsOffsetInRange(aOffset) && aOffset + aLength <= EndOffset();
    }
    bool IsOverlappingWith(uint32_t aOffset, uint32_t aLength) const {
      if (!HasRects() || aOffset == UINT32_MAX || !aLength) {
        return false;
      }
      CheckedInt<uint32_t> endOffset = CheckedInt<uint32_t>(aOffset) + aLength;
      if (NS_WARN_IF(!endOffset.isValid())) {
        return false;
      }
      return aOffset < EndOffset() && endOffset.value() > mStart;
    }
    LayoutDeviceIntRect GetRect(uint32_t aOffset) const;
    LayoutDeviceIntRect GetUnionRect(uint32_t aOffset, uint32_t aLength) const;
    LayoutDeviceIntRect GetUnionRectAsFarAsPossible(
        uint32_t aOffset, uint32_t aLength, bool aRoundToExistingOffset) const;

    friend std::ostream& operator<<(std::ostream& aStream,
                                    const TextRectArray& aTextRectArray) {
      aStream << "{ mStart=" << aTextRectArray.mStart
              << ", mRects={ Length()=" << aTextRectArray.Length();
      if (aTextRectArray.HasRects()) {
        aStream << ", Elements()=[ ";
        static constexpr uint32_t kMaxPrintRects = 4;
        const uint32_t kFirstHalf = aTextRectArray.Length() <= kMaxPrintRects
                                        ? UINT32_MAX
                                        : (kMaxPrintRects + 1) / 2;
        const uint32_t kSecondHalf =
            aTextRectArray.Length() <= kMaxPrintRects ? 0 : kMaxPrintRects / 2;
        for (uint32_t i = 0; i < aTextRectArray.Length(); i++) {
          if (i > 0) {
            aStream << ", ";
          }
          aStream << ToString(aTextRectArray.mRects[i]).c_str();
          if (i + 1 == kFirstHalf) {
            aStream << " ...";
            i = aTextRectArray.Length() - kSecondHalf - 1;
          }
        }
      }
      return aStream << " ] } }";
    }

   private:
    // For ParamTraits<TextRectArray>
    TextRectArray() = default;

    friend struct IPC::ParamTraits<ContentCache::TextRectArray>;
    ALLOW_DEPRECATED_READPARAM
  };
  Maybe<TextRectArray> mTextRectArray;
  Maybe<TextRectArray> mLastCommitStringTextRectArray;

  LayoutDeviceIntRect mEditorRect;

  friend class ContentCacheInParent;
  friend struct IPC::ParamTraits<ContentCache>;
  friend struct IPC::ParamTraits<ContentCache::Selection>;
  friend struct IPC::ParamTraits<ContentCache::Caret>;
  friend struct IPC::ParamTraits<ContentCache::TextRectArray>;
  friend std::ostream& operator<<(
      std::ostream& aStream,
      const Selection& aSelection);  // For e(Prev|Next)CharRect
  ALLOW_DEPRECATED_READPARAM
};

class ContentCacheInChild final : public ContentCache {
 public:
  ContentCacheInChild() = default;

  /**
   * Called when composition event will be dispatched in this process from
   * PuppetWidget.
   */
  void OnCompositionEvent(const WidgetCompositionEvent& aCompositionEvent);

  /**
   * When IME loses focus, this should be called and making this forget the
   * content for reducing footprint.
   */
  void Clear();

  /**
   * Cache*() retrieves the latest content information and store them.
   * Be aware, CacheSelection() calls CacheCaretAndTextRects(),
   * CacheCaretAndTextRects() calls CacheCaret() and CacheTextRects(), and
   * CacheText() calls CacheSelection().  So, related data is also retrieved
   * automatically.
   */
  bool CacheEditorRect(nsIWidget* aWidget,
                       const IMENotification* aNotification = nullptr);
  bool CacheCaretAndTextRects(nsIWidget* aWidget,
                              const IMENotification* aNotification = nullptr);
  bool CacheText(nsIWidget* aWidget,
                 const IMENotification* aNotification = nullptr);

  bool CacheAll(nsIWidget* aWidget,
                const IMENotification* aNotification = nullptr);

  /**
   * SetSelection() modifies selection with specified raw data. And also this
   * tries to retrieve text rects too.
   *
   * @return true if the selection is cached.  Otherwise, false.
   */
  [[nodiscard]] bool SetSelection(
      nsIWidget* aWidget,
      const IMENotification::SelectionChangeDataBase& aSelectionChangeData);

 private:
  bool QueryCharRect(nsIWidget* aWidget, uint32_t aOffset,
                     LayoutDeviceIntRect& aCharRect) const;
  bool QueryCharRectArray(nsIWidget* aWidget, uint32_t aOffset,
                          uint32_t aLength, RectArray& aCharRectArray) const;
  bool CacheSelection(nsIWidget* aWidget,
                      const IMENotification* aNotification = nullptr);
  bool CacheCaret(nsIWidget* aWidget,
                  const IMENotification* aNotification = nullptr);
  bool CacheTextRects(nsIWidget* aWidget,
                      const IMENotification* aNotification = nullptr);

  // Once composition is committed, all of the commit string may be composed
  // again by Kakutei-Undo of Japanese IME.  Therefore, we need to keep
  // storing the last composition start to cache all character rects of the
  // last commit string.
  Maybe<OffsetAndData<uint32_t>> mLastCommit;
};

class ContentCacheInParent final : public ContentCache {
 public:
  ContentCacheInParent() = delete;
  explicit ContentCacheInParent(dom::BrowserParent& aBrowserParent);

  /**
   * AssignContent() is called when BrowserParent receives ContentCache from
   * the content process.  This doesn't copy composition information because
   * it's managed by BrowserParent itself.
   */
  void AssignContent(const ContentCache& aOther, nsIWidget* aWidget,
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
   * For eQueryTextRect, fail if cached offset/length aren't equals to input.
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
   *          BrowserParent or aWidget.  Therefore, the caller must not destroy
   *          this instance during a call of this method.
   */
  void OnEventNeedingAckHandled(nsIWidget* aWidget, EventMessage aMessage,
                                uint32_t aCompositionId);

  /**
   * RequestIMEToCommitComposition() requests aWidget to commit or cancel
   * composition.  If it's handled synchronously, this returns true.
   *
   * @param aWidget     The widget to be requested to commit or cancel
   *                    the composition.
   * @param aCancel     When the caller tries to cancel the composition, true.
   *                    Otherwise, i.e., tries to commit the composition, false.
   * @param aCompositionId
   *                    The composition ID which should be committed or
   *                    canceled.
   * @param aCommittedString    The committed string (i.e., the last data of
   *                            dispatched composition events during requesting
   *                            IME to commit composition.
   * @return            Whether the composition is actually committed
   *                    synchronously.
   */
  bool RequestIMEToCommitComposition(nsIWidget* aWidget, bool aCancel,
                                     uint32_t aCompositionId,
                                     nsAString& aCommittedString);

  /**
   * MaybeNotifyIME() may notify IME of the notification.  If child process
   * hasn't been handled all sending events yet, this stores the notification
   * and flush it later.
   */
  void MaybeNotifyIME(nsIWidget* aWidget, const IMENotification& aNotification);

 private:
  struct HandlingCompositionData;

  // Return true when the widget in this process thinks that IME has
  // composition.  So, this returns true when there is at least one handling
  // composition data and the last handling composition has not dispatched
  // composition commit event to the remote process yet.
  [[nodiscard]] bool WidgetHasComposition() const {
    return !mHandlingCompositions.IsEmpty() &&
           !mHandlingCompositions.LastElement().mSentCommitEvent;
  }

  // Return true if there is a pending composition which has already sent
  // a commit event to the remote process, but not yet handled by it.
  [[nodiscard]] bool HasPendingCommit() const {
    for (const HandlingCompositionData& data : mHandlingCompositions) {
      if (data.mSentCommitEvent) {
        return true;
      }
    }
    return false;
  }

  // Return the number of composition events and set selection events which were
  // sent to the remote process, but we've not verified that the remote process
  // finished handling it.
  [[nodiscard]] uint32_t PendingEventsNeedingAck() const {
    uint32_t ret = mPendingSetSelectionEventNeedingAck;
    for (const HandlingCompositionData& data : mHandlingCompositions) {
      ret += data.mPendingEventsNeedingAck;
    }
    return ret;
  }

  [[nodiscard]] HandlingCompositionData* GetHandlingCompositionData(
      uint32_t aCompositionId) {
    for (HandlingCompositionData& data : mHandlingCompositions) {
      if (data.mCompositionId == aCompositionId) {
        return &data;
      }
    }
    return nullptr;
  }
  [[nodiscard]] const HandlingCompositionData* GetHandlingCompositionData(
      uint32_t aCompositionId) const {
    return const_cast<ContentCacheInParent*>(this)->GetHandlingCompositionData(
        aCompositionId);
  }

  IMENotification mPendingSelectionChange;
  IMENotification mPendingTextChange;
  IMENotification mPendingLayoutChange;
  IMENotification mPendingCompositionUpdate;

#if MOZ_DIAGNOSTIC_ASSERT_ENABLED
  // Log of event messages to be output to crash report.
  nsTArray<EventMessage> mDispatchedEventMessages;
  nsTArray<EventMessage> mReceivedEventMessages;
  // Log of RequestIMEToCommitComposition() in the last 2 compositions.
  enum class RequestIMEToCommitCompositionResult : uint8_t {
    eToOldCompositionReceived,
    eToUnknownCompositionReceived,
    eToCommittedCompositionReceived,
    eReceivedAfterBrowserParentBlur,
    eReceivedButNoTextComposition,
    eReceivedButForDifferentTextComposition,
    eHandledAsynchronously,
    eHandledSynchronously,
  };
  const char* ToReadableText(
      RequestIMEToCommitCompositionResult aResult) const {
    switch (aResult) {
      case RequestIMEToCommitCompositionResult::eToOldCompositionReceived:
        return "Commit request is not handled because it's for "
               "older composition";
      case RequestIMEToCommitCompositionResult::eToUnknownCompositionReceived:
        return "Commit request is not handled because it's for "
               "unknown composition";
      case RequestIMEToCommitCompositionResult::eToCommittedCompositionReceived:
        return "Commit request is not handled because BrowserParent has "
               "already "
               "sent commit event for the composition";
      case RequestIMEToCommitCompositionResult::eReceivedAfterBrowserParentBlur:
        return "Commit request is handled with stored composition string "
               "because BrowserParent has already lost focus";
      case RequestIMEToCommitCompositionResult::eReceivedButNoTextComposition:
        return "Commit request is not handled because there is no "
               "TextComposition instance";
      case RequestIMEToCommitCompositionResult::
          eReceivedButForDifferentTextComposition:
        return "Commit request is handled with stored composition string "
               "because new TextComposition is active";
      case RequestIMEToCommitCompositionResult::eHandledAsynchronously:
        return "Commit request is handled but IME doesn't commit current "
               "composition synchronously";
      case RequestIMEToCommitCompositionResult::eHandledSynchronously:
        return "Commit request is handled synchronously";
      default:
        return "Unknown reason";
    }
  }
  nsTArray<RequestIMEToCommitCompositionResult>
      mRequestIMEToCommitCompositionResults;
#endif  // MOZ_DIAGNOSTIC_ASSERT_ENABLED

  // Stores pending compositions (meaning eCompositionStart was dispatched, but
  // eCompositionCommit(AsIs) has not been handled by the remote process yet).
  struct HandlingCompositionData {
    // The lasted composition string which was sent to the remote process.
    nsString mCompositionString;
    // The composition ID of a handling composition with the instance.
    uint32_t mCompositionId;
    // Increased when sending composition events and decreased when the
    // remote process finished handling the events.
    uint32_t mPendingEventsNeedingAck = 0u;
    // true if eCompositionCommit(AsIs) has already been sent to the remote
    // process.
    bool mSentCommitEvent = false;

    explicit HandlingCompositionData(uint32_t aCompositionId)
        : mCompositionId(aCompositionId) {}
  };
  AutoTArray<HandlingCompositionData, 2> mHandlingCompositions;

  // mBrowserParent is owner of the instance.
  dom::BrowserParent& MOZ_NON_OWNING_REF mBrowserParent;
  // This is not nullptr only while the instance is requesting IME to
  // composition.  Then, data value of dispatched composition events should
  // be stored into the instance.
  nsAString* mCommitStringByRequest;
  // mCompositionStartInChild stores current composition start offset in the
  // remote process.
  Maybe<uint32_t> mCompositionStartInChild;
  // Increased when sending eSetSelection events and decreased when the remote
  // process finished handling the events.  Note that eSetSelection may be
  // dispatched without composition.  Therefore, we need to count it with this.
  uint32_t mPendingSetSelectionEventNeedingAck = 0u;
  // mPendingCommitLength is commit string length of the first pending
  // composition.  This is used by relative offset query events when querying
  // new composition start offset.
  // Note that when mHandlingCompositions has 2 or more elements, i.e., there
  // are 2 or more pending compositions, this cache won't be used because in
  // such case, anyway ContentCacheInParent cannot return proper character rect.
  uint32_t mPendingCommitLength;
  // mIsChildIgnoringCompositionEvents is set to true if the child process
  // requests commit composition whose commit has already been sent to it.
  // Then, set to false when the child process ignores the commit event.
  bool mIsChildIgnoringCompositionEvents;

  /**
   * When following methods' aRoundToExistingOffset is true, even if specified
   * offset or range is out of bounds, the result is computed with the existing
   * cache forcibly.
   */
  bool GetCaretRect(uint32_t aOffset, bool aRoundToExistingOffset,
                    LayoutDeviceIntRect& aCaretRect) const;
  bool GetTextRect(uint32_t aOffset, bool aRoundToExistingOffset,
                   LayoutDeviceIntRect& aTextRect) const;
  bool GetUnionTextRects(uint32_t aOffset, uint32_t aLength,
                         bool aRoundToExistingOffset,
                         LayoutDeviceIntRect& aUnionTextRect) const;

  void FlushPendingNotifications(nsIWidget* aWidget);

#if MOZ_DIAGNOSTIC_ASSERT_ENABLED
  /**
   * Remove unnecessary messages from mDispatchedEventMessages and
   * mReceivedEventMessages.
   */
  void RemoveUnnecessaryEventMessageLog();

  /**
   * Append event message log to aLog.
   */
  void AppendEventMessageLog(nsACString& aLog) const;
#endif  // #if MOZ_DIAGNOSTIC_ASSERT_ENABLED
};

}  // namespace mozilla

#endif  // mozilla_ContentCache_h
