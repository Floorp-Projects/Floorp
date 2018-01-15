/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ContentCache.h"

#include "mozilla/IMEStateManager.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Logging.h"
#include "mozilla/Move.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TextComposition.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/TabParent.h"
#include "nsExceptionHandler.h"
#include "nsIWidget.h"

namespace mozilla {

using namespace dom;
using namespace widget;

static const char*
GetBoolName(bool aBool)
{
  return aBool ? "true" : "false";
}

static const char*
GetNotificationName(const IMENotification* aNotification)
{
  if (!aNotification) {
    return "Not notification";
  }
  return ToChar(aNotification->mMessage);
}

class GetRectText : public nsAutoCString
{
public:
  explicit GetRectText(const LayoutDeviceIntRect& aRect)
  {
    AssignLiteral("{ x=");
    AppendInt(aRect.X());
    AppendLiteral(", y=");
    AppendInt(aRect.Y());
    AppendLiteral(", width=");
    AppendInt(aRect.Width());
    AppendLiteral(", height=");
    AppendInt(aRect.Height());
    AppendLiteral(" }");
  }
  virtual ~GetRectText() {}
};

class GetWritingModeName : public nsAutoCString
{
public:
  explicit GetWritingModeName(const WritingMode& aWritingMode)
  {
    if (!aWritingMode.IsVertical()) {
      AssignLiteral("Horizontal");
      return;
    }
    if (aWritingMode.IsVerticalLR()) {
      AssignLiteral("Vertical (LTR)");
      return;
    }
    AssignLiteral("Vertical (RTL)");
  }
  virtual ~GetWritingModeName() {}
};

class GetEscapedUTF8String final : public NS_ConvertUTF16toUTF8
{
public:
  explicit GetEscapedUTF8String(const nsAString& aString)
    : NS_ConvertUTF16toUTF8(aString)
  {
    Escape();
  }
  explicit GetEscapedUTF8String(const char16ptr_t aString)
    : NS_ConvertUTF16toUTF8(aString)
  {
    Escape();
  }
  GetEscapedUTF8String(const char16ptr_t aString, uint32_t aLength)
    : NS_ConvertUTF16toUTF8(aString, aLength)
  {
    Escape();
  }

private:
  void Escape()
  {
    ReplaceSubstring("\r", "\\r");
    ReplaceSubstring("\n", "\\n");
    ReplaceSubstring("\t", "\\t");
  }
};

/*****************************************************************************
 * mozilla::ContentCache
 *****************************************************************************/

LazyLogModule sContentCacheLog("ContentCacheWidgets");

ContentCache::ContentCache()
  : mCompositionStart(UINT32_MAX)
{
}

/*****************************************************************************
 * mozilla::ContentCacheInChild
 *****************************************************************************/

ContentCacheInChild::ContentCacheInChild()
  : ContentCache()
{
}

void
ContentCacheInChild::Clear()
{
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("0x%p Clear()", this));

  mCompositionStart = UINT32_MAX;
  mText.Truncate();
  mSelection.Clear();
  mFirstCharRect.SetEmpty();
  mCaret.Clear();
  mTextRectArray.Clear();
  mEditorRect.SetEmpty();
}

bool
ContentCacheInChild::CacheAll(nsIWidget* aWidget,
                              const IMENotification* aNotification)
{
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("0x%p CacheAll(aWidget=0x%p, aNotification=%s)",
     this, aWidget, GetNotificationName(aNotification)));

  if (NS_WARN_IF(!CacheText(aWidget, aNotification)) ||
      NS_WARN_IF(!CacheEditorRect(aWidget, aNotification))) {
    return false;
  }
  return true;
}

bool
ContentCacheInChild::CacheSelection(nsIWidget* aWidget,
                                    const IMENotification* aNotification)
{
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("0x%p CacheSelection(aWidget=0x%p, aNotification=%s)",
     this, aWidget, GetNotificationName(aNotification)));

  mCaret.Clear();
  mSelection.Clear();

  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetQueryContentEvent selection(true, eQuerySelectedText, aWidget);
  aWidget->DispatchEvent(&selection, status);
  if (NS_WARN_IF(!selection.mSucceeded)) {
    MOZ_LOG(sContentCacheLog, LogLevel::Error,
      ("0x%p CacheSelection(), FAILED, "
       "couldn't retrieve the selected text", this));
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

  return CacheCaret(aWidget, aNotification) &&
         CacheTextRects(aWidget, aNotification);
}

bool
ContentCacheInChild::CacheCaret(nsIWidget* aWidget,
                                const IMENotification* aNotification)
{
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("0x%p CacheCaret(aWidget=0x%p, aNotification=%s)",
     this, aWidget, GetNotificationName(aNotification)));

  mCaret.Clear();

  if (NS_WARN_IF(!mSelection.IsValid())) {
    return false;
  }

  // XXX Should be mSelection.mFocus?
  mCaret.mOffset = mSelection.StartOffset();

  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetQueryContentEvent caretRect(true, eQueryCaretRect, aWidget);
  caretRect.InitForQueryCaretRect(mCaret.mOffset);
  aWidget->DispatchEvent(&caretRect, status);
  if (NS_WARN_IF(!caretRect.mSucceeded)) {
    MOZ_LOG(sContentCacheLog, LogLevel::Error,
      ("0x%p CacheCaret(), FAILED, "
       "couldn't retrieve the caret rect at offset=%u",
       this, mCaret.mOffset));
    mCaret.Clear();
    return false;
  }
  mCaret.mRect = caretRect.mReply.mRect;
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("0x%p CacheCaret(), Succeeded, "
     "mSelection={ mAnchor=%u, mFocus=%u, mWritingMode=%s }, "
     "mCaret={ mOffset=%u, mRect=%s }",
     this, mSelection.mAnchor, mSelection.mFocus,
     GetWritingModeName(mSelection.mWritingMode).get(), mCaret.mOffset,
     GetRectText(mCaret.mRect).get()));
  return true;
}

bool
ContentCacheInChild::CacheEditorRect(nsIWidget* aWidget,
                                     const IMENotification* aNotification)
{
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("0x%p CacheEditorRect(aWidget=0x%p, aNotification=%s)",
     this, aWidget, GetNotificationName(aNotification)));

  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetQueryContentEvent editorRectEvent(true, eQueryEditorRect, aWidget);
  aWidget->DispatchEvent(&editorRectEvent, status);
  if (NS_WARN_IF(!editorRectEvent.mSucceeded)) {
    MOZ_LOG(sContentCacheLog, LogLevel::Error,
      ("0x%p CacheEditorRect(), FAILED, "
       "couldn't retrieve the editor rect", this));
    return false;
  }
  mEditorRect = editorRectEvent.mReply.mRect;
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("0x%p CacheEditorRect(), Succeeded, "
     "mEditorRect=%s", this, GetRectText(mEditorRect).get()));
  return true;
}

bool
ContentCacheInChild::CacheText(nsIWidget* aWidget,
                               const IMENotification* aNotification)
{
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("0x%p CacheText(aWidget=0x%p, aNotification=%s)",
     this, aWidget, GetNotificationName(aNotification)));

  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetQueryContentEvent queryText(true, eQueryTextContent, aWidget);
  queryText.InitForQueryTextContent(0, UINT32_MAX);
  aWidget->DispatchEvent(&queryText, status);
  if (NS_WARN_IF(!queryText.mSucceeded)) {
    MOZ_LOG(sContentCacheLog, LogLevel::Error,
      ("0x%p CacheText(), FAILED, couldn't retrieve whole text", this));
    mText.Truncate();
    return false;
  }
  mText = queryText.mReply.mString;
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("0x%p CacheText(), Succeeded, mText.Length()=%u", this, mText.Length()));

  return CacheSelection(aWidget, aNotification);
}

bool
ContentCacheInChild::QueryCharRect(nsIWidget* aWidget,
                                   uint32_t aOffset,
                                   LayoutDeviceIntRect& aCharRect) const
{
  aCharRect.SetEmpty();

  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetQueryContentEvent textRect(true, eQueryTextRect, aWidget);
  textRect.InitForQueryTextRect(aOffset, 1);
  aWidget->DispatchEvent(&textRect, status);
  if (NS_WARN_IF(!textRect.mSucceeded)) {
    return false;
  }
  aCharRect = textRect.mReply.mRect;

  // Guarantee the rect is not empty.
  if (NS_WARN_IF(!aCharRect.Height())) {
    aCharRect.SetHeight(1);
  }
  if (NS_WARN_IF(!aCharRect.Width())) {
    aCharRect.SetWidth(1);
  }
  return true;
}

bool
ContentCacheInChild::QueryCharRectArray(nsIWidget* aWidget,
                                        uint32_t aOffset,
                                        uint32_t aLength,
                                        RectArray& aCharRectArray) const
{
  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetQueryContentEvent textRects(true, eQueryTextRectArray, aWidget);
  textRects.InitForQueryTextRectArray(aOffset, aLength);
  aWidget->DispatchEvent(&textRects, status);
  if (NS_WARN_IF(!textRects.mSucceeded)) {
    aCharRectArray.Clear();
    return false;
  }
  aCharRectArray = Move(textRects.mReply.mRectArray);
  return true;
}

bool
ContentCacheInChild::CacheTextRects(nsIWidget* aWidget,
                                    const IMENotification* aNotification)
{
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("0x%p CacheTextRects(aWidget=0x%p, aNotification=%s), "
     "mCaret={ mOffset=%u, IsValid()=%s }",
     this, aWidget, GetNotificationName(aNotification), mCaret.mOffset,
     GetBoolName(mCaret.IsValid())));

  mCompositionStart = UINT32_MAX;
  mTextRectArray.Clear();
  mSelection.ClearAnchorCharRects();
  mSelection.ClearFocusCharRects();
  mSelection.mRect.SetEmpty();
  mFirstCharRect.SetEmpty();

  if (NS_WARN_IF(!mSelection.IsValid())) {
    return false;
  }

  // Retrieve text rects in composition string if there is.
  RefPtr<TextComposition> textComposition =
    IMEStateManager::GetTextCompositionFor(aWidget);
  if (textComposition) {
    // mCompositionStart may be updated by some composition event handlers.
    // So, let's update it with the latest information.
    mCompositionStart = textComposition->NativeOffsetOfStartComposition();
    // Note that TextComposition::String() may not be modified here because
    // it's modified after all edit action listeners are performed but this
    // is called while some of them are performed.
    // FYI: For supporting IME which commits composition and restart new
    //      composition immediately, we should cache next character of current
    //      composition too.
    uint32_t length = textComposition->LastData().Length() + 1;
    mTextRectArray.mStart = mCompositionStart;
    if (NS_WARN_IF(!QueryCharRectArray(aWidget, mTextRectArray.mStart, length,
                                       mTextRectArray.mRects))) {
      MOZ_LOG(sContentCacheLog, LogLevel::Error,
        ("0x%p CacheTextRects(), FAILED, "
         "couldn't retrieve text rect array of the composition string", this));
    }
  }

  if (mTextRectArray.InRange(mSelection.mAnchor) &&
      (!mSelection.mAnchor || mTextRectArray.InRange(mSelection.mAnchor - 1))) {
    mSelection.mAnchorCharRects[eNextCharRect] =
      mTextRectArray.GetRect(mSelection.mAnchor);
    if (mSelection.mAnchor) {
      mSelection.mAnchorCharRects[ePrevCharRect] =
        mTextRectArray.GetRect(mSelection.mAnchor - 1);
    }
  } else {
    RectArray rects;
    uint32_t startOffset = mSelection.mAnchor ? mSelection.mAnchor - 1 : 0;
    uint32_t length = mSelection.mAnchor ? 2 : 1;
    if (NS_WARN_IF(!QueryCharRectArray(aWidget, startOffset, length, rects))) {
      MOZ_LOG(sContentCacheLog, LogLevel::Error,
        ("0x%p CacheTextRects(), FAILED, "
         "couldn't retrieve text rect array around the selection anchor (%u)",
         this, mSelection.mAnchor));
      MOZ_ASSERT(mSelection.mAnchorCharRects[ePrevCharRect].IsEmpty());
      MOZ_ASSERT(mSelection.mAnchorCharRects[eNextCharRect].IsEmpty());
    } else {
      if (rects.Length() > 1) {
        mSelection.mAnchorCharRects[ePrevCharRect] = rects[0];
        mSelection.mAnchorCharRects[eNextCharRect] = rects[1];
      } else if (rects.Length()) {
        mSelection.mAnchorCharRects[eNextCharRect] = rects[0];
        MOZ_ASSERT(mSelection.mAnchorCharRects[ePrevCharRect].IsEmpty());
      }
    }
  }

  if (mSelection.Collapsed()) {
    mSelection.mFocusCharRects[0] = mSelection.mAnchorCharRects[0];
    mSelection.mFocusCharRects[1] = mSelection.mAnchorCharRects[1];
  } else if (mTextRectArray.InRange(mSelection.mFocus) &&
             (!mSelection.mFocus ||
              mTextRectArray.InRange(mSelection.mFocus - 1))) {
    mSelection.mFocusCharRects[eNextCharRect] =
      mTextRectArray.GetRect(mSelection.mFocus);
    if (mSelection.mFocus) {
      mSelection.mFocusCharRects[ePrevCharRect] =
        mTextRectArray.GetRect(mSelection.mFocus - 1);
    }
  } else {
    RectArray rects;
    uint32_t startOffset = mSelection.mFocus ? mSelection.mFocus - 1 : 0;
    uint32_t length = mSelection.mFocus ? 2 : 1;
    if (NS_WARN_IF(!QueryCharRectArray(aWidget, startOffset, length, rects))) {
      MOZ_LOG(sContentCacheLog, LogLevel::Error,
        ("0x%p CacheTextRects(), FAILED, "
         "couldn't retrieve text rect array around the selection focus (%u)",
         this, mSelection.mFocus));
      MOZ_ASSERT(mSelection.mFocusCharRects[ePrevCharRect].IsEmpty());
      MOZ_ASSERT(mSelection.mFocusCharRects[eNextCharRect].IsEmpty());
    } else {
      if (rects.Length() > 1) {
        mSelection.mFocusCharRects[ePrevCharRect] = rects[0];
        mSelection.mFocusCharRects[eNextCharRect] = rects[1];
      } else if (rects.Length()) {
        mSelection.mFocusCharRects[eNextCharRect] = rects[0];
        MOZ_ASSERT(mSelection.mFocusCharRects[ePrevCharRect].IsEmpty());
      }
    }
  }

  if (!mSelection.Collapsed()) {
    nsEventStatus status = nsEventStatus_eIgnore;
    WidgetQueryContentEvent textRect(true, eQueryTextRect, aWidget);
    textRect.InitForQueryTextRect(mSelection.StartOffset(),
                                  mSelection.Length());
    aWidget->DispatchEvent(&textRect, status);
    if (NS_WARN_IF(!textRect.mSucceeded)) {
      MOZ_LOG(sContentCacheLog, LogLevel::Error,
        ("0x%p CacheTextRects(), FAILED, "
         "couldn't retrieve text rect of whole selected text", this));
    } else {
      mSelection.mRect = textRect.mReply.mRect;
    }
  }

  if (!mSelection.mFocus) {
    mFirstCharRect = mSelection.mFocusCharRects[eNextCharRect];
  } else if (mSelection.mFocus == 1) {
    mFirstCharRect = mSelection.mFocusCharRects[ePrevCharRect];
  } else if (!mSelection.mAnchor) {
    mFirstCharRect = mSelection.mAnchorCharRects[eNextCharRect];
  } else if (mSelection.mAnchor == 1) {
    mFirstCharRect = mSelection.mFocusCharRects[ePrevCharRect];
  } else if (mTextRectArray.InRange(0)) {
    mFirstCharRect = mTextRectArray.GetRect(0);
  } else {
    LayoutDeviceIntRect charRect;
    if (NS_WARN_IF(!QueryCharRect(aWidget, 0, charRect))) {
      MOZ_LOG(sContentCacheLog, LogLevel::Error,
        ("0x%p CacheTextRects(), FAILED, "
         "couldn't retrieve first char rect", this));
    } else {
      mFirstCharRect = charRect;
    }
  }

  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("0x%p CacheTextRects(), Succeeded, "
     "mText.Length()=%x, mTextRectArray={ mStart=%u, mRects.Length()=%zu"
     " }, mSelection={ mAnchor=%u, mAnchorCharRects[eNextCharRect]=%s, "
     "mAnchorCharRects[ePrevCharRect]=%s, mFocus=%u, "
     "mFocusCharRects[eNextCharRect]=%s, mFocusCharRects[ePrevCharRect]=%s, "
     "mRect=%s }, mFirstCharRect=%s",
     this, mText.Length(), mTextRectArray.mStart,
     mTextRectArray.mRects.Length(), mSelection.mAnchor,
     GetRectText(mSelection.mAnchorCharRects[eNextCharRect]).get(),
     GetRectText(mSelection.mAnchorCharRects[ePrevCharRect]).get(),
     mSelection.mFocus,
     GetRectText(mSelection.mFocusCharRects[eNextCharRect]).get(),
     GetRectText(mSelection.mFocusCharRects[ePrevCharRect]).get(),
     GetRectText(mSelection.mRect).get(), GetRectText(mFirstCharRect).get()));
  return true;
}

void
ContentCacheInChild::SetSelection(nsIWidget* aWidget,
                                  uint32_t aStartOffset,
                                  uint32_t aLength,
                                  bool aReversed,
                                  const WritingMode& aWritingMode)
{
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("0x%p SetSelection(aStartOffset=%u, "
     "aLength=%u, aReversed=%s, aWritingMode=%s), mText.Length()=%u",
     this, aStartOffset, aLength, GetBoolName(aReversed),
     GetWritingModeName(aWritingMode).get(), mText.Length()));

  if (!aReversed) {
    mSelection.mAnchor = aStartOffset;
    mSelection.mFocus = aStartOffset + aLength;
  } else {
    mSelection.mAnchor = aStartOffset + aLength;
    mSelection.mFocus = aStartOffset;
  }
  mSelection.mWritingMode = aWritingMode;

  if (NS_WARN_IF(!CacheCaret(aWidget))) {
    return;
  }
  Unused << NS_WARN_IF(!CacheTextRects(aWidget));
}

/*****************************************************************************
 * mozilla::ContentCacheInParent
 *****************************************************************************/

ContentCacheInParent::ContentCacheInParent(TabParent& aTabParent)
  : ContentCache()
  , mTabParent(aTabParent)
  , mCommitStringByRequest(nullptr)
  , mPendingEventsNeedingAck(0)
  , mCompositionStartInChild(UINT32_MAX)
  , mPendingCommitLength(0)
  , mPendingCompositionCount(0)
  , mPendingCommitCount(0)
  , mWidgetHasComposition(false)
  , mIsChildIgnoringCompositionEvents(false)
{
}

void
ContentCacheInParent::AssignContent(const ContentCache& aOther,
                                    nsIWidget* aWidget,
                                    const IMENotification* aNotification)
{
  mText = aOther.mText;
  mSelection = aOther.mSelection;
  mFirstCharRect = aOther.mFirstCharRect;
  mCaret = aOther.mCaret;
  mTextRectArray = aOther.mTextRectArray;
  mEditorRect = aOther.mEditorRect;

  // Only when there is one composition, the TextComposition instance in this
  // process is managing the composition in the remote process.  Therefore,
  // we shouldn't update composition start offset of TextComposition with
  // old composition which is still being handled by the child process.
  if (mWidgetHasComposition && mPendingCompositionCount == 1) {
    IMEStateManager::MaybeStartOffsetUpdatedInChild(aWidget, mCompositionStart);
  }

  // When this instance allows to query content relative to composition string,
  // we should modify mCompositionStart with the latest information in the
  // remote process because now we have the information around the composition
  // string.
  mCompositionStartInChild = aOther.mCompositionStart;
  if (mWidgetHasComposition || mPendingCommitCount) {
    if (aOther.mCompositionStart != UINT32_MAX) {
      if (mCompositionStart != aOther.mCompositionStart) {
        mCompositionStart = aOther.mCompositionStart;
        mPendingCommitLength = 0;
      }
    } else if (mCompositionStart != mSelection.StartOffset()) {
      mCompositionStart = mSelection.StartOffset();
      mPendingCommitLength = 0;
      NS_WARNING_ASSERTION(mCompositionStart != UINT32_MAX,
                           "mCompositionStart shouldn't be invalid offset when "
                           "the widget has composition");
    }
  }

  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("0x%p AssignContent(aNotification=%s), "
     "Succeeded, mText.Length()=%u, mSelection={ mAnchor=%u, mFocus=%u, "
     "mWritingMode=%s, mAnchorCharRects[eNextCharRect]=%s, "
     "mAnchorCharRects[ePrevCharRect]=%s, mFocusCharRects[eNextCharRect]=%s, "
     "mFocusCharRects[ePrevCharRect]=%s, mRect=%s }, "
     "mFirstCharRect=%s, mCaret={ mOffset=%u, mRect=%s }, mTextRectArray={ "
     "mStart=%u, mRects.Length()=%zu }, mWidgetHasComposition=%s, "
     "mPendingCompositionCount=%u, mCompositionStart=%u, "
     "mPendingCommitLength=%u, mEditorRect=%s",
     this, GetNotificationName(aNotification),
     mText.Length(), mSelection.mAnchor, mSelection.mFocus,
     GetWritingModeName(mSelection.mWritingMode).get(),
     GetRectText(mSelection.mAnchorCharRects[eNextCharRect]).get(),
     GetRectText(mSelection.mAnchorCharRects[ePrevCharRect]).get(),
     GetRectText(mSelection.mFocusCharRects[eNextCharRect]).get(),
     GetRectText(mSelection.mFocusCharRects[ePrevCharRect]).get(),
     GetRectText(mSelection.mRect).get(), GetRectText(mFirstCharRect).get(),
     mCaret.mOffset, GetRectText(mCaret.mRect).get(), mTextRectArray.mStart,
     mTextRectArray.mRects.Length(), GetBoolName(mWidgetHasComposition),
     mPendingCompositionCount, mCompositionStart, mPendingCommitLength,
     GetRectText(mEditorRect).get()));
}

bool
ContentCacheInParent::HandleQueryContentEvent(WidgetQueryContentEvent& aEvent,
                                              nsIWidget* aWidget) const
{
  MOZ_ASSERT(aWidget);

  aEvent.mSucceeded = false;
  aEvent.mReply.mFocusedWidget = aWidget;

  // ContentCache doesn't store offset of its start with XP linebreaks.
  // So, we don't support to query contents relative to composition start
  // offset with XP linebreaks.
  if (NS_WARN_IF(!aEvent.mUseNativeLineBreak)) {
    MOZ_LOG(sContentCacheLog, LogLevel::Error,
      ("0x%p HandleQueryContentEvent(), FAILED due to query with XP linebreaks",
       this));
    return false;
  }

  if (NS_WARN_IF(!aEvent.mInput.IsValidOffset())) {
    MOZ_LOG(sContentCacheLog, LogLevel::Error,
      ("0x%p HandleQueryContentEvent(), FAILED due to invalid offset",
       this));
    return false;
  }

  if (NS_WARN_IF(!aEvent.mInput.IsValidEventMessage(aEvent.mMessage))) {
    MOZ_LOG(sContentCacheLog, LogLevel::Error,
      ("0x%p HandleQueryContentEvent(), FAILED due to invalid event message",
       this));
    return false;
  }

  bool isRelativeToInsertionPoint = aEvent.mInput.mRelativeToInsertionPoint;
  if (isRelativeToInsertionPoint) {
    if (aWidget->PluginHasFocus()) {
      if (NS_WARN_IF(!aEvent.mInput.MakeOffsetAbsolute(0))) {
        MOZ_LOG(sContentCacheLog, LogLevel::Error,
          ("0x%p HandleQueryContentEvent(), FAILED due to "
           "aEvent.mInput.MakeOffsetAbsolute(0) failure, aEvent={ mMessage=%s, "
           "mInput={ mOffset=%" PRId64 ", mLength=%" PRIu32 " } }",
           this, ToChar(aEvent.mMessage), aEvent.mInput.mOffset,
           aEvent.mInput.mLength));
        return false;
      }
    } else if (mWidgetHasComposition || mPendingCommitCount) {
      if (NS_WARN_IF(!aEvent.mInput.MakeOffsetAbsolute(
                                      mCompositionStart +
                                        mPendingCommitLength))) {
        MOZ_LOG(sContentCacheLog, LogLevel::Error,
          ("0x%p HandleQueryContentEvent(), FAILED due to "
           "aEvent.mInput.MakeOffsetAbsolute(mCompositionStart + "
           "mPendingCommitLength) failure, "
           "mCompositionStart=%" PRIu32 ", mPendingCommitLength=%" PRIu32 ", "
           "aEvent={ mMessage=%s, mInput={ mOffset=%" PRId64
           ", mLength=%" PRIu32 " } }",
           this, mCompositionStart, mPendingCommitLength,
           ToChar(aEvent.mMessage), aEvent.mInput.mOffset,
           aEvent.mInput.mLength));
        return false;
      }
    } else if (NS_WARN_IF(!mSelection.IsValid())) {
      MOZ_LOG(sContentCacheLog, LogLevel::Error,
        ("0x%p HandleQueryContentEvent(), FAILED due to mSelection is invalid",
         this));
      return false;
    } else if (NS_WARN_IF(!aEvent.mInput.MakeOffsetAbsolute(
                                           mSelection.StartOffset() +
                                             mPendingCommitLength))) {
      MOZ_LOG(sContentCacheLog, LogLevel::Error,
        ("0x%p HandleQueryContentEvent(), FAILED due to "
         "aEvent.mInput.MakeOffsetAbsolute(mSelection.StartOffset() + "
         "mPendingCommitLength) failure, "
         "mSelection={ StartOffset()=%d, Length()=%d }, "
         "mPendingCommitLength=%" PRIu32 ", aEvent={ mMessage=%s, "
         "mInput={ mOffset=%" PRId64 ", mLength=%" PRIu32 " } }",
         this, mSelection.StartOffset(), mSelection.Length(),
         mPendingCommitLength, ToChar(aEvent.mMessage),
         aEvent.mInput.mOffset, aEvent.mInput.mLength));
      return false;
    }
  }

  switch (aEvent.mMessage) {
    case eQuerySelectedText:
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
        ("0x%p HandleQueryContentEvent("
         "aEvent={ mMessage=eQuerySelectedText }, aWidget=0x%p)",
         this, aWidget));
      if (aWidget->PluginHasFocus()) {
        MOZ_LOG(sContentCacheLog, LogLevel::Info,
          ("0x%p HandleQueryContentEvent(), "
           "return emtpy selection becasue plugin has focus",
           this));
        aEvent.mSucceeded = true;
        aEvent.mReply.mOffset = 0;
        aEvent.mReply.mReversed = false;
        aEvent.mReply.mHasSelection = false;
        return true;
      }
      if (NS_WARN_IF(!IsSelectionValid())) {
        // If content cache hasn't been initialized properly, make the query
        // failed.
        MOZ_LOG(sContentCacheLog, LogLevel::Error,
          ("0x%p HandleQueryContentEvent(), "
           "FAILED because mSelection is not valid", this));
        return true;
      }
      aEvent.mReply.mOffset = mSelection.StartOffset();
      if (mSelection.Collapsed()) {
        aEvent.mReply.mString.Truncate(0);
      } else {
        if (NS_WARN_IF(mSelection.EndOffset() > mText.Length())) {
          MOZ_LOG(sContentCacheLog, LogLevel::Error,
            ("0x%p HandleQueryContentEvent(), "
             "FAILED because mSelection.EndOffset()=%u is larger than "
             "mText.Length()=%u",
             this, mSelection.EndOffset(), mText.Length()));
          return false;
        }
        aEvent.mReply.mString =
          Substring(mText, aEvent.mReply.mOffset, mSelection.Length());
      }
      aEvent.mReply.mReversed = mSelection.Reversed();
      aEvent.mReply.mHasSelection = true;
      aEvent.mReply.mWritingMode = mSelection.mWritingMode;
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
        ("0x%p HandleQueryContentEvent(), "
         "Succeeded, aEvent={ mReply={ mOffset=%u, mString=\"%s\", "
         "mReversed=%s, mHasSelection=%s, mWritingMode=%s } }",
         this, aEvent.mReply.mOffset,
         GetEscapedUTF8String(aEvent.mReply.mString).get(),
         GetBoolName(aEvent.mReply.mReversed),
         GetBoolName(aEvent.mReply.mHasSelection),
         GetWritingModeName(aEvent.mReply.mWritingMode).get()));
      break;
    case eQueryTextContent: {
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
        ("0x%p HandleQueryContentEvent("
         "aEvent={ mMessage=eQueryTextContent, mInput={ mOffset=%" PRId64
         ", mLength=%u } }, aWidget=0x%p), mText.Length()=%u",
         this, aEvent.mInput.mOffset,
         aEvent.mInput.mLength, aWidget, mText.Length()));
      uint32_t inputOffset = aEvent.mInput.mOffset;
      uint32_t inputEndOffset =
        std::min(aEvent.mInput.EndOffset(), mText.Length());
      if (NS_WARN_IF(inputEndOffset < inputOffset)) {
        MOZ_LOG(sContentCacheLog, LogLevel::Error,
          ("0x%p HandleQueryContentEvent(), "
           "FAILED because inputOffset=%u is larger than inputEndOffset=%u",
           this, inputOffset, inputEndOffset));
        return false;
      }
      aEvent.mReply.mOffset = inputOffset;
      aEvent.mReply.mString =
        Substring(mText, inputOffset, inputEndOffset - inputOffset);
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
        ("0x%p HandleQueryContentEvent(), "
         "Succeeded, aEvent={ mReply={ mOffset=%u, mString.Length()=%u } }",
         this, aEvent.mReply.mOffset, aEvent.mReply.mString.Length()));
      break;
    }
    case eQueryTextRect:
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
        ("0x%p HandleQueryContentEvent("
         "aEvent={ mMessage=eQueryTextRect, mInput={ mOffset=%" PRId64
         ", mLength=%u } }, aWidget=0x%p), mText.Length()=%u",
         this, aEvent.mInput.mOffset, aEvent.mInput.mLength, aWidget,
         mText.Length()));
      if (NS_WARN_IF(!IsSelectionValid())) {
        // If content cache hasn't been initialized properly, make the query
        // failed.
        MOZ_LOG(sContentCacheLog, LogLevel::Error,
          ("0x%p HandleQueryContentEvent(), "
           "FAILED because mSelection is not valid", this));
        return true;
      }
      // Note that if the query is relative to insertion point, the query was
      // probably requested by native IME.  In such case, we should return
      // non-empty rect since returning failure causes IME showing its window
      // at odd position.
      if (aEvent.mInput.mLength) {
        if (NS_WARN_IF(!GetUnionTextRects(aEvent.mInput.mOffset,
                                          aEvent.mInput.mLength,
                                          isRelativeToInsertionPoint,
                                          aEvent.mReply.mRect))) {
          // XXX We don't have cache for this request.
          MOZ_LOG(sContentCacheLog, LogLevel::Error,
            ("0x%p HandleQueryContentEvent(), "
             "FAILED to get union rect", this));
          return false;
        }
      } else {
        // If the length is 0, we should return caret rect instead.
        if (NS_WARN_IF(!GetCaretRect(aEvent.mInput.mOffset,
                                     isRelativeToInsertionPoint,
                                     aEvent.mReply.mRect))) {
          MOZ_LOG(sContentCacheLog, LogLevel::Error,
            ("0x%p HandleQueryContentEvent(), "
             "FAILED to get caret rect", this));
          return false;
        }
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
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
        ("0x%p HandleQueryContentEvent(), "
         "Succeeded, aEvent={ mReply={ mOffset=%u, mString=\"%s\", "
         "mWritingMode=%s, mRect=%s } }",
         this, aEvent.mReply.mOffset,
         GetEscapedUTF8String(aEvent.mReply.mString).get(),
         GetWritingModeName(aEvent.mReply.mWritingMode).get(),
         GetRectText(aEvent.mReply.mRect).get()));
      break;
    case eQueryCaretRect:
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
        ("0x%p HandleQueryContentEvent("
         "aEvent={ mMessage=eQueryCaretRect, mInput={ mOffset=%" PRId64 " } }, "
         "aWidget=0x%p), mText.Length()=%u",
         this, aEvent.mInput.mOffset, aWidget, mText.Length()));
      if (NS_WARN_IF(!IsSelectionValid())) {
        // If content cache hasn't been initialized properly, make the query
        // failed.
        MOZ_LOG(sContentCacheLog, LogLevel::Error,
          ("0x%p HandleQueryContentEvent(), "
           "FAILED because mSelection is not valid", this));
        return true;
      }
      // Note that if the query is relative to insertion point, the query was
      // probably requested by native IME.  In such case, we should return
      // non-empty rect since returning failure causes IME showing its window
      // at odd position.
      if (NS_WARN_IF(!GetCaretRect(aEvent.mInput.mOffset,
                                   isRelativeToInsertionPoint,
                                   aEvent.mReply.mRect))) {
        MOZ_LOG(sContentCacheLog, LogLevel::Error,
          ("0x%p HandleQueryContentEvent(), "
           "FAILED to get caret rect", this));
        return false;
      }
      aEvent.mReply.mOffset = aEvent.mInput.mOffset;
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
        ("0x%p HandleQueryContentEvent(), "
         "Succeeded, aEvent={ mReply={ mOffset=%u, mRect=%s } }",
         this, aEvent.mReply.mOffset, GetRectText(aEvent.mReply.mRect).get()));
      break;
    case eQueryEditorRect:
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
        ("0x%p HandleQueryContentEvent("
         "aEvent={ mMessage=eQueryEditorRect }, aWidget=0x%p)",
         this, aWidget));
      aEvent.mReply.mRect = mEditorRect;
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
        ("0x%p HandleQueryContentEvent(), "
         "Succeeded, aEvent={ mReply={ mRect=%s } }",
         this, GetRectText(aEvent.mReply.mRect).get()));
      break;
    default:
      break;
  }
  aEvent.mSucceeded = true;
  return true;
}

bool
ContentCacheInParent::GetTextRect(uint32_t aOffset,
                                  bool aRoundToExistingOffset,
                                  LayoutDeviceIntRect& aTextRect) const
{
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("0x%p GetTextRect(aOffset=%u, "
     "aRoundToExistingOffset=%s), "
     "mTextRectArray={ mStart=%u, mRects.Length()=%zu }, "
     "mSelection={ mAnchor=%u, mFocus=%u }",
     this, aOffset, GetBoolName(aRoundToExistingOffset),
     mTextRectArray.mStart, mTextRectArray.mRects.Length(),
     mSelection.mAnchor, mSelection.mFocus));

  if (!aOffset) {
    NS_WARNING_ASSERTION(!mFirstCharRect.IsEmpty(), "empty rect");
    aTextRect = mFirstCharRect;
    return !aTextRect.IsEmpty();
  }
  if (aOffset == mSelection.mAnchor) {
    NS_WARNING_ASSERTION(!mSelection.mAnchorCharRects[eNextCharRect].IsEmpty(),
                         "empty rect");
    aTextRect = mSelection.mAnchorCharRects[eNextCharRect];
    return !aTextRect.IsEmpty();
  }
  if (mSelection.mAnchor && aOffset == mSelection.mAnchor - 1) {
    NS_WARNING_ASSERTION(!mSelection.mAnchorCharRects[ePrevCharRect].IsEmpty(),
                         "empty rect");
    aTextRect = mSelection.mAnchorCharRects[ePrevCharRect];
    return !aTextRect.IsEmpty();
  }
  if (aOffset == mSelection.mFocus) {
    NS_WARNING_ASSERTION(!mSelection.mFocusCharRects[eNextCharRect].IsEmpty(),
                         "empty rect");
    aTextRect = mSelection.mFocusCharRects[eNextCharRect];
    return !aTextRect.IsEmpty();
  }
  if (mSelection.mFocus && aOffset == mSelection.mFocus - 1) {
    NS_WARNING_ASSERTION(!mSelection.mFocusCharRects[ePrevCharRect].IsEmpty(),
                         "empty rect");
    aTextRect = mSelection.mFocusCharRects[ePrevCharRect];
    return !aTextRect.IsEmpty();
  }

  uint32_t offset = aOffset;
  if (!mTextRectArray.InRange(aOffset)) {
    if (!aRoundToExistingOffset) {
      aTextRect.SetEmpty();
      return false;
    }
    if (!mTextRectArray.IsValid()) {
      // If there are no rects in mTextRectArray, we should refer the start of
      // the selection because IME must query a char rect around it if there is
      // no composition.
      aTextRect = mSelection.StartCharRect();
      return !aTextRect.IsEmpty();
    }
    if (offset < mTextRectArray.StartOffset()) {
      offset = mTextRectArray.StartOffset();
    } else {
      offset = mTextRectArray.EndOffset() - 1;
    }
  }
  aTextRect = mTextRectArray.GetRect(offset);
  return !aTextRect.IsEmpty();
}

bool
ContentCacheInParent::GetUnionTextRects(
                        uint32_t aOffset,
                        uint32_t aLength,
                        bool aRoundToExistingOffset,
                        LayoutDeviceIntRect& aUnionTextRect) const
{
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("0x%p GetUnionTextRects(aOffset=%u, "
     "aLength=%u, aRoundToExistingOffset=%s), mTextRectArray={ "
     "mStart=%u, mRects.Length()=%zu }, "
     "mSelection={ mAnchor=%u, mFocus=%u }",
     this, aOffset, aLength, GetBoolName(aRoundToExistingOffset),
     mTextRectArray.mStart, mTextRectArray.mRects.Length(),
     mSelection.mAnchor, mSelection.mFocus));

  CheckedInt<uint32_t> endOffset =
    CheckedInt<uint32_t>(aOffset) + aLength;
  if (!endOffset.isValid()) {
    return false;
  }

  if (!mSelection.Collapsed() &&
      aOffset == mSelection.StartOffset() && aLength == mSelection.Length()) {
    NS_WARNING_ASSERTION(!mSelection.mRect.IsEmpty(), "empty rect");
    aUnionTextRect = mSelection.mRect;
    return !aUnionTextRect.IsEmpty();
  }

  if (aLength == 1) {
    if (!aOffset) {
      NS_WARNING_ASSERTION(!mFirstCharRect.IsEmpty(), "empty rect");
      aUnionTextRect = mFirstCharRect;
      return !aUnionTextRect.IsEmpty();
    }
    if (aOffset == mSelection.mAnchor) {
      NS_WARNING_ASSERTION(
        !mSelection.mAnchorCharRects[eNextCharRect].IsEmpty(), "empty rect");
      aUnionTextRect = mSelection.mAnchorCharRects[eNextCharRect];
      return !aUnionTextRect.IsEmpty();
    }
    if (mSelection.mAnchor && aOffset == mSelection.mAnchor - 1) {
      NS_WARNING_ASSERTION(
        !mSelection.mAnchorCharRects[ePrevCharRect].IsEmpty(), "empty rect");
      aUnionTextRect = mSelection.mAnchorCharRects[ePrevCharRect];
      return !aUnionTextRect.IsEmpty();
    }
    if (aOffset == mSelection.mFocus) {
      NS_WARNING_ASSERTION(
        !mSelection.mFocusCharRects[eNextCharRect].IsEmpty(), "empty rect");
      aUnionTextRect = mSelection.mFocusCharRects[eNextCharRect];
      return !aUnionTextRect.IsEmpty();
    }
    if (mSelection.mFocus && aOffset == mSelection.mFocus - 1) {
      NS_WARNING_ASSERTION(
        !mSelection.mFocusCharRects[ePrevCharRect].IsEmpty(), "empty rect");
      aUnionTextRect = mSelection.mFocusCharRects[ePrevCharRect];
      return !aUnionTextRect.IsEmpty();
    }
  }

  // Even if some text rects are not cached of the queried range,
  // we should return union rect when the first character's rect is cached
  // since the first character rect is important and the others are not so
  // in most cases.

  if (!aOffset && aOffset != mSelection.mAnchor &&
      aOffset != mSelection.mFocus && !mTextRectArray.InRange(aOffset)) {
    // The first character rect isn't cached.
    return false;
  }

  if ((aRoundToExistingOffset && mTextRectArray.HasRects()) ||
      mTextRectArray.IsOverlappingWith(aOffset, aLength)) {
    aUnionTextRect =
      mTextRectArray.GetUnionRectAsFarAsPossible(aOffset, aLength,
                                                 aRoundToExistingOffset);
  } else {
    aUnionTextRect.SetEmpty();
  }

  if (!aOffset) {
    aUnionTextRect = aUnionTextRect.Union(mFirstCharRect);
  }
  if (aOffset <= mSelection.mAnchor && mSelection.mAnchor < endOffset.value()) {
    aUnionTextRect =
      aUnionTextRect.Union(mSelection.mAnchorCharRects[eNextCharRect]);
  }
  if (mSelection.mAnchor && aOffset <= mSelection.mAnchor - 1 &&
      mSelection.mAnchor - 1 < endOffset.value()) {
    aUnionTextRect =
      aUnionTextRect.Union(mSelection.mAnchorCharRects[ePrevCharRect]);
  }
  if (aOffset <= mSelection.mFocus && mSelection.mFocus < endOffset.value()) {
    aUnionTextRect =
      aUnionTextRect.Union(mSelection.mFocusCharRects[eNextCharRect]);
  }
  if (mSelection.mFocus && aOffset <= mSelection.mFocus - 1 &&
      mSelection.mFocus - 1 < endOffset.value()) {
    aUnionTextRect =
      aUnionTextRect.Union(mSelection.mFocusCharRects[ePrevCharRect]);
  }

  return !aUnionTextRect.IsEmpty();
}

bool
ContentCacheInParent::GetCaretRect(uint32_t aOffset,
                                   bool aRoundToExistingOffset,
                                   LayoutDeviceIntRect& aCaretRect) const
{
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("0x%p GetCaretRect(aOffset=%u, "
     "aRoundToExistingOffset=%s), "
     "mCaret={ mOffset=%u, mRect=%s, IsValid()=%s }, mTextRectArray={ "
     "mStart=%u, mRects.Length()=%zu }, mSelection={ mAnchor=%u, mFocus=%u, "
     "mWritingMode=%s, mAnchorCharRects[eNextCharRect]=%s, "
     "mAnchorCharRects[ePrevCharRect]=%s, mFocusCharRects[eNextCharRect]=%s, "
     "mFocusCharRects[ePrevCharRect]=%s }, mFirstCharRect=%s",
     this, aOffset, GetBoolName(aRoundToExistingOffset),
     mCaret.mOffset, GetRectText(mCaret.mRect).get(),
     GetBoolName(mCaret.IsValid()), mTextRectArray.mStart,
     mTextRectArray.mRects.Length(), mSelection.mAnchor, mSelection.mFocus,
     GetWritingModeName(mSelection.mWritingMode).get(),
     GetRectText(mSelection.mAnchorCharRects[eNextCharRect]).get(),
     GetRectText(mSelection.mAnchorCharRects[ePrevCharRect]).get(),
     GetRectText(mSelection.mFocusCharRects[eNextCharRect]).get(),
     GetRectText(mSelection.mFocusCharRects[ePrevCharRect]).get(),
     GetRectText(mFirstCharRect).get()));

  if (mCaret.IsValid() && mCaret.mOffset == aOffset) {
    aCaretRect = mCaret.mRect;
    return true;
  }

  // Guess caret rect from the text rect if it's stored.
  if (!GetTextRect(aOffset, aRoundToExistingOffset, aCaretRect)) {
    // There might be previous character rect in the cache.  If so, we can
    // guess the caret rect with it.
    if (!aOffset ||
        !GetTextRect(aOffset - 1, aRoundToExistingOffset, aCaretRect)) {
      aCaretRect.SetEmpty();
      return false;
    }

    if (mSelection.mWritingMode.IsVertical()) {
      aCaretRect.MoveToY(aCaretRect.YMost());
    } else {
      // XXX bidi-unaware.
      aCaretRect.MoveToX(aCaretRect.XMost());
    }
  }

  // XXX This is not bidi aware because we don't cache each character's
  //     direction.  However, this is usually used by IME, so, assuming the
  //     character is in LRT context must not cause any problem.
  if (mSelection.mWritingMode.IsVertical()) {
    aCaretRect.SetHeight(mCaret.IsValid() ? mCaret.mRect.Height() : 1);
  } else {
    aCaretRect.SetWidth(mCaret.IsValid() ? mCaret.mRect.Width() : 1);
  }
  return true;
}

bool
ContentCacheInParent::OnCompositionEvent(const WidgetCompositionEvent& aEvent)
{
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("0x%p OnCompositionEvent(aEvent={ "
     "mMessage=%s, mData=\"%s\" (Length()=%u), mRanges->Length()=%zu }), "
     "mPendingEventsNeedingAck=%u, mWidgetHasComposition=%s, "
     "mPendingCompositionCount=%" PRIu8 ", mPendingCommitCount=%" PRIu8 ", "
     "mIsChildIgnoringCompositionEvents=%s, mCommitStringByRequest=0x%p",
     this, ToChar(aEvent.mMessage),
     GetEscapedUTF8String(aEvent.mData).get(), aEvent.mData.Length(),
     aEvent.mRanges ? aEvent.mRanges->Length() : 0, mPendingEventsNeedingAck,
     GetBoolName(mWidgetHasComposition), mPendingCompositionCount,
     mPendingCommitCount, GetBoolName(mIsChildIgnoringCompositionEvents),
     mCommitStringByRequest));

#if MOZ_DIAGNOSTIC_ASSERT_ENABLED
  mDispatchedEventMessages.AppendElement(aEvent.mMessage);
#endif // #ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED

  // We must be able to simulate the selection because
  // we might not receive selection updates in time
  if (!mWidgetHasComposition) {
    if (aEvent.mWidget && aEvent.mWidget->PluginHasFocus()) {
      // If focus is on plugin, we cannot get selection range
      mCompositionStart = 0;
    } else if (mCompositionStartInChild != UINT32_MAX) {
      // If there is pending composition in the remote process, let's use
      // its start offset temporarily because this stores a lot of information
      // around it and the user must look around there, so, showing some UI
      // around it must make sense.
      mCompositionStart = mCompositionStartInChild;
    } else {
      mCompositionStart = mSelection.StartOffset();
    }
    MOZ_ASSERT(aEvent.mMessage == eCompositionStart);
    MOZ_RELEASE_ASSERT(mPendingCompositionCount < UINT8_MAX);
    mPendingCompositionCount++;
  }

  mWidgetHasComposition = !aEvent.CausesDOMCompositionEndEvent();

  if (!mWidgetHasComposition) {
    // mCompositionStart will be reset when commit event is completely handled
    // in the remote process.
    if (mPendingCompositionCount == 1) {
      mPendingCommitLength = aEvent.mData.Length();
    }
    mPendingCommitCount++;
  } else if (aEvent.mMessage != eCompositionStart) {
    mCompositionString = aEvent.mData;
  }

  // During REQUEST_TO_COMMIT_COMPOSITION or REQUEST_TO_CANCEL_COMPOSITION,
  // widget usually sends a eCompositionChange and/or eCompositionCommit event
  // to finalize or clear the composition, respectively.  In this time,
  // we need to intercept all composition events here and pass the commit
  // string for returning to the remote process as a result of
  // RequestIMEToCommitComposition().  Then, eCommitComposition event will
  // be dispatched with the committed string in the remote process internally.
  if (mCommitStringByRequest) {
    MOZ_ASSERT(aEvent.mMessage == eCompositionChange ||
               aEvent.mMessage == eCompositionCommit);
    *mCommitStringByRequest = aEvent.mData;
    // We need to wait eCompositionCommitRequestHandled from the remote process
    // in this case.  Therefore, mPendingEventsNeedingAck needs to be
    // incremented here.  Additionally, we stop sending eCompositionCommit(AsIs)
    // event.  Therefore, we need to decrement mPendingCommitCount which has
    // been incremented above.
    if (!mWidgetHasComposition) {
      mPendingEventsNeedingAck++;
      MOZ_DIAGNOSTIC_ASSERT(mPendingCommitCount);
      if (mPendingCommitCount) {
        mPendingCommitCount--;
      }
    }
    return false;
  }

  mPendingEventsNeedingAck++;
  return true;
}

void
ContentCacheInParent::OnSelectionEvent(
                        const WidgetSelectionEvent& aSelectionEvent)
{
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("0x%p OnSelectionEvent(aEvent={ "
     "mMessage=%s, mOffset=%u, mLength=%u, mReversed=%s, "
     "mExpandToClusterBoundary=%s, mUseNativeLineBreak=%s }), "
     "mPendingEventsNeedingAck=%u, mWidgetHasComposition=%s, "
     "mPendingCompositionCount=%" PRIu8 ", mPendingCommitCount=%" PRIu8 ", "
     "mIsChildIgnoringCompositionEvents=%s",
     this, ToChar(aSelectionEvent.mMessage),
     aSelectionEvent.mOffset, aSelectionEvent.mLength,
     GetBoolName(aSelectionEvent.mReversed),
     GetBoolName(aSelectionEvent.mExpandToClusterBoundary),
     GetBoolName(aSelectionEvent.mUseNativeLineBreak), mPendingEventsNeedingAck,
     GetBoolName(mWidgetHasComposition), mPendingCompositionCount,
     mPendingCommitCount, GetBoolName(mIsChildIgnoringCompositionEvents)));

#if MOZ_DIAGNOSTIC_ASSERT_ENABLED
  mDispatchedEventMessages.AppendElement(aSelectionEvent.mMessage);
#endif // MOZ_DIAGNOSTIC_ASSERT_ENABLED

  mPendingEventsNeedingAck++;
}

void
ContentCacheInParent::OnEventNeedingAckHandled(nsIWidget* aWidget,
                                                EventMessage aMessage)
{
  // This is called when the child process receives WidgetCompositionEvent or
  // WidgetSelectionEvent.

  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("0x%p OnEventNeedingAckHandled(aWidget=0x%p, "
     "aMessage=%s), mPendingEventsNeedingAck=%u, "
     "mWidgetHasComposition=%s, mPendingCompositionCount=%" PRIu8 ", "
     "mPendingCommitCount=%" PRIu8 ", mIsChildIgnoringCompositionEvents=%s",
     this, aWidget, ToChar(aMessage), mPendingEventsNeedingAck,
     GetBoolName(mWidgetHasComposition), mPendingCompositionCount,
     mPendingCommitCount, GetBoolName(mIsChildIgnoringCompositionEvents)));

#if MOZ_DIAGNOSTIC_ASSERT_ENABLED
  mReceivedEventMessages.AppendElement(aMessage);
#endif // #if MOZ_DIAGNOSTIC_ASSERT_ENABLED

  bool isCommittedInChild =
    // Commit requester in the remote process has committed the composition.
    aMessage == eCompositionCommitRequestHandled ||
    // The commit event has been handled normally in the remote process.
    (!mIsChildIgnoringCompositionEvents &&
     WidgetCompositionEvent::IsFollowedByCompositionEnd(aMessage));

  if (isCommittedInChild) {
#if MOZ_DIAGNOSTIC_ASSERT_ENABLED
    if (mPendingCompositionCount == 1) {
      RemoveUnnecessaryEventMessageLog();
    }
#endif // #if MOZ_DIAGNOSTIC_ASSERT_ENABLED

    if (NS_WARN_IF(!mPendingCompositionCount)) {
#if MOZ_DIAGNOSTIC_ASSERT_ENABLED
      nsPrintfCString info("\nThere is no pending composition but received %s "
                           "message from the remote child\n\n",
                           ToChar(aMessage));
      AppendEventMessageLog(info);
      CrashReporter::AppendAppNotesToCrashReport(info);
      MOZ_DIAGNOSTIC_ASSERT(false,
        "No pending composition but received unexpected commit event");
#endif // #if MOZ_DIAGNOSTIC_ASSERT_ENABLED

      // Prevent odd behavior in release channel.
      mPendingCompositionCount = 1;
    }

    mPendingCompositionCount--;

    // Forget composition string only when the latest composition string is
    // handled in the remote process because if there is 2 or more pending
    // composition, this value shouldn't be referred.
    if (!mPendingCompositionCount) {
      mCompositionString.Truncate();
    }

    // Forget pending commit string length if it's handled in the remote
    // process.  Note that this doesn't care too old composition's commit
    // string because in such case, we cannot return proper information
    // to IME synchornously.
    mPendingCommitLength = 0;
  }

  if (WidgetCompositionEvent::IsFollowedByCompositionEnd(aMessage)) {
    // After the remote process receives eCompositionCommit(AsIs) event,
    // it'll restart to handle composition events.
    mIsChildIgnoringCompositionEvents = false;

    if (NS_WARN_IF(!mPendingCommitCount)) {
#if MOZ_DIAGNOSTIC_ASSERT_ENABLED
      nsPrintfCString info("\nThere is no pending comment events but received "
                           "%s message from the remote child\n\n",
                           ToChar(aMessage));
      AppendEventMessageLog(info);
      CrashReporter::AppendAppNotesToCrashReport(info);
      MOZ_DIAGNOSTIC_ASSERT(false,
        "No pending commit events but received unexpected commit event");
#endif // #if MOZ_DIAGNOSTIC_ASSERT_ENABLED

      // Prevent odd behavior in release channel.
      mPendingCommitCount = 1;
    }

    mPendingCommitCount--;
  } else if (aMessage == eCompositionCommitRequestHandled &&
             mPendingCommitCount) {
    // If the remote process commits composition synchronously after
    // requesting commit composition and we've already sent commit composition,
    // it starts to ignore following composition events until receiving
    // eCompositionStart event.
    mIsChildIgnoringCompositionEvents = true;
  }

  // If neither widget (i.e., IME) nor the remote process has composition,
  // now, we can forget composition string informations.
  if (!mWidgetHasComposition &&
      !mPendingCompositionCount && !mPendingCommitCount) {
    mCompositionStart = UINT32_MAX;
  }

  if (NS_WARN_IF(!mPendingEventsNeedingAck)) {
#if MOZ_DIAGNOSTIC_ASSERT_ENABLED
    nsPrintfCString info("\nThere is no pending events but received %s "
                         "message from the remote child\n\n",
                         ToChar(aMessage));
    AppendEventMessageLog(info);
    CrashReporter::AppendAppNotesToCrashReport(info);
#endif // #if MOZ_DIAGNOSTIC_ASSERT_ENABLED
    MOZ_DIAGNOSTIC_ASSERT(false,
      "No pending event message but received unexpected event");
    mPendingEventsNeedingAck = 1;
  }
  if (--mPendingEventsNeedingAck) {
    return;
  }

  FlushPendingNotifications(aWidget);
}

bool
ContentCacheInParent::RequestIMEToCommitComposition(nsIWidget* aWidget,
                                                    bool aCancel,
                                                    nsAString& aCommittedString)
{
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("0x%p RequestToCommitComposition(aWidget=%p, "
     "aCancel=%s), mPendingCompositionCount=%" PRIu8 ", "
     "mPendingCommitCount=%" PRIu8 ", mIsChildIgnoringCompositionEvents=%s, "
     "IMEStateManager::DoesTabParentHaveIMEFocus(&mTabParent)=%s, "
     "mWidgetHasComposition=%s, mCommitStringByRequest=%p",
     this, aWidget, GetBoolName(aCancel), mPendingCompositionCount,
     mPendingCommitCount, GetBoolName(mIsChildIgnoringCompositionEvents),
     GetBoolName(IMEStateManager::DoesTabParentHaveIMEFocus(&mTabParent)),
     GetBoolName(mWidgetHasComposition), mCommitStringByRequest));

  MOZ_ASSERT(!mCommitStringByRequest);

  // If there are 2 or more pending compositions, we already sent
  // eCompositionCommit(AsIs) to the remote process.  So, this request is
  // too late for IME.  The remote process should wait following
  // composition events for cleaning up TextComposition and handle the
  // request as it's handled asynchronously.
  if (mPendingCompositionCount > 1) {
#if MOZ_DIAGNOSTIC_ASSERT_ENABLED
    mRequestIMEToCommitCompositionResults.
      AppendElement(RequestIMEToCommitCompositionResult::
                      eToOldCompositionReceived);
#endif // #if MOZ_DIAGNOSTIC_ASSERT_ENABLED
    return false;
  }

  // If there is no pending composition, we may have already sent
  // eCompositionCommit(AsIs) event for the active composition.  If so, the
  // remote process will receive composition events which causes cleaning up
  // TextComposition.  So, this shouldn't do nothing and TextComposition
  // should handle the request as it's handled asynchronously.
  // XXX Perhaps, this is wrong because TextComposition in child process
  //     may commit the composition with current composition string in the
  //     remote process.  I.e., it may be different from actual commit string
  //     which user typed.  So, perhaps, we should return true and the commit
  //     string.
  if (mPendingCommitCount) {
#if MOZ_DIAGNOSTIC_ASSERT_ENABLED
    mRequestIMEToCommitCompositionResults.
      AppendElement(RequestIMEToCommitCompositionResult::
                      eToCommittedCompositionReceived);
#endif // #if MOZ_DIAGNOSTIC_ASSERT_ENABLED
    return false;
  }

  // If TabParent which has IME focus was already changed to different one, the
  // request shouldn't be sent to IME because it's too late.
  if (!IMEStateManager::DoesTabParentHaveIMEFocus(&mTabParent)) {
    // Use the latest composition string which may not be handled in the
    // remote process for avoiding data loss.
#if MOZ_DIAGNOSTIC_ASSERT_ENABLED
    mRequestIMEToCommitCompositionResults.
      AppendElement(RequestIMEToCommitCompositionResult::
                      eReceivedAfterTabParentBlur);
#endif // #if MOZ_DIAGNOSTIC_ASSERT_ENABLED
    aCommittedString = mCompositionString;
    // After we return true from here, i.e., without actually requesting IME
    // to commit composition, we will receive eCompositionCommitRequestHandled
    // pseudo event message from the remote process.  So, we need to increment
    // mPendingEventsNeedingAck here.
    mPendingEventsNeedingAck++;
    return true;
  }

  RefPtr<TextComposition> composition =
    IMEStateManager::GetTextCompositionFor(aWidget);
  if (NS_WARN_IF(!composition)) {
    MOZ_LOG(sContentCacheLog, LogLevel::Warning,
      ("  0x%p RequestToCommitComposition(), "
       "does nothing due to no composition", this));
#if MOZ_DIAGNOSTIC_ASSERT_ENABLED
    mRequestIMEToCommitCompositionResults.
      AppendElement(RequestIMEToCommitCompositionResult::
                      eReceivedButNoTextComposition);
#endif // #if MOZ_DIAGNOSTIC_ASSERT_ENABLED
    return false;
  }

  mCommitStringByRequest = &aCommittedString;

  // Request commit or cancel composition with TextComposition because we may
  // have already requested to commit or cancel the composition or we may
  // have already received eCompositionCommit(AsIs) event.  Those status are
  // managed by composition.  So, if we don't request commit composition,
  // we should do nothing with native IME here.
  composition->RequestToCommit(aWidget, aCancel);

  mCommitStringByRequest = nullptr;

  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("  0x%p RequestToCommitComposition(), "
     "mWidgetHasComposition=%s, the composition %s committed synchronously",
     this, GetBoolName(mWidgetHasComposition),
     composition->Destroyed() ? "WAS" : "has NOT been"));

  if (!composition->Destroyed()) {
    // When the composition isn't committed synchronously, the remote process's
    // TextComposition instance will synthesize commit events and wait to
    // receive delayed composition events.  When TextComposition instances both
    // in this process and the remote process will be destroyed when delayed
    // composition events received. TextComposition instance in the parent
    // process will dispatch following composition events and be destroyed
    // normally. On the other hand, TextComposition instance in the remote
    // process won't dispatch following composition events and will be
    // destroyed by IMEStateManager::DispatchCompositionEvent().
#if MOZ_DIAGNOSTIC_ASSERT_ENABLED
    mRequestIMEToCommitCompositionResults.
      AppendElement(RequestIMEToCommitCompositionResult::
                      eHandledAsynchronously);
#endif // #if MOZ_DIAGNOSTIC_ASSERT_ENABLED
    return false;
  }

  // When the composition is committed synchronously, the commit string will be
  // returned to the remote process.  Then, PuppetWidget will dispatch
  // eCompositionCommit event with the returned commit string (i.e., the value
  // is aCommittedString of this method) and that causes destroying
  // TextComposition instance in the remote process (Note that TextComposition
  // instance in this process was already destroyed).
#if MOZ_DIAGNOSTIC_ASSERT_ENABLED
  mRequestIMEToCommitCompositionResults.
    AppendElement(RequestIMEToCommitCompositionResult::eHandledSynchronously);
#endif // #if MOZ_DIAGNOSTIC_ASSERT_ENABLED
  return true;
}

void
ContentCacheInParent::MaybeNotifyIME(nsIWidget* aWidget,
                                     const IMENotification& aNotification)
{
  if (!mPendingEventsNeedingAck) {
    IMEStateManager::NotifyIME(aNotification, aWidget, &mTabParent);
    return;
  }

  switch (aNotification.mMessage) {
    case NOTIFY_IME_OF_SELECTION_CHANGE:
      mPendingSelectionChange.MergeWith(aNotification);
      break;
    case NOTIFY_IME_OF_TEXT_CHANGE:
      mPendingTextChange.MergeWith(aNotification);
      break;
    case NOTIFY_IME_OF_POSITION_CHANGE:
      mPendingLayoutChange.MergeWith(aNotification);
      break;
    case NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED:
      mPendingCompositionUpdate.MergeWith(aNotification);
      break;
    default:
      MOZ_CRASH("Unsupported notification");
      break;
  }
}

void
ContentCacheInParent::FlushPendingNotifications(nsIWidget* aWidget)
{
  MOZ_ASSERT(!mPendingEventsNeedingAck);

  // If the TabParent's widget has already gone, this can do nothing since
  // widget is necessary to notify IME of something.
  if (!aWidget) {
    return;
  }

  // New notifications which are notified during flushing pending notifications
  // should be merged again.
  mPendingEventsNeedingAck++;

  nsCOMPtr<nsIWidget> widget = aWidget;

  // First, text change notification should be sent because selection change
  // notification notifies IME of current selection range in the latest content.
  // So, IME may need the latest content before that.
  if (mPendingTextChange.HasNotification()) {
    IMENotification notification(mPendingTextChange);
    if (!widget->Destroyed()) {
      mPendingTextChange.Clear();
      IMEStateManager::NotifyIME(notification, widget, &mTabParent);
    }
  }

  if (mPendingSelectionChange.HasNotification()) {
    IMENotification notification(mPendingSelectionChange);
    if (!widget->Destroyed()) {
      mPendingSelectionChange.Clear();
      IMEStateManager::NotifyIME(notification, widget, &mTabParent);
    }
  }

  // Layout change notification should be notified after selection change
  // notification because IME may want to query position of new caret position.
  if (mPendingLayoutChange.HasNotification()) {
    IMENotification notification(mPendingLayoutChange);
    if (!widget->Destroyed()) {
      mPendingLayoutChange.Clear();
      IMEStateManager::NotifyIME(notification, widget, &mTabParent);
    }
  }

  // Finally, send composition update notification because it notifies IME of
  // finishing handling whole sending events.
  if (mPendingCompositionUpdate.HasNotification()) {
    IMENotification notification(mPendingCompositionUpdate);
    if (!widget->Destroyed()) {
      mPendingCompositionUpdate.Clear();
      IMEStateManager::NotifyIME(notification, widget, &mTabParent);
    }
  }

  if (!--mPendingEventsNeedingAck && !widget->Destroyed() &&
      (mPendingTextChange.HasNotification() ||
       mPendingSelectionChange.HasNotification() ||
       mPendingLayoutChange.HasNotification() ||
       mPendingCompositionUpdate.HasNotification())) {
    FlushPendingNotifications(widget);
  }
}

#if MOZ_DIAGNOSTIC_ASSERT_ENABLED

void
ContentCacheInParent::RemoveUnnecessaryEventMessageLog()
{
  bool foundLastCompositionStart = false;
  for (size_t i = mDispatchedEventMessages.Length(); i > 1; i--) {
    if (mDispatchedEventMessages[i - 1] != eCompositionStart) {
      continue;
    }
    if (!foundLastCompositionStart) {
      // Find previous eCompositionStart of the latest eCompositionStart.
      foundLastCompositionStart = true;
      continue;
    }
    // Remove the messages before the last 2 sets of composition events.
    mDispatchedEventMessages.RemoveElementsAt(0, i - 1);
    break;
  }
  uint32_t numberOfCompositionCommitRequestHandled = 0;
  foundLastCompositionStart = false;
  for (size_t i = mReceivedEventMessages.Length(); i > 1; i--) {
    if (mReceivedEventMessages[i - 1] == eCompositionCommitRequestHandled) {
      numberOfCompositionCommitRequestHandled++;
    }
    if (mReceivedEventMessages[i - 1] != eCompositionStart) {
      continue;
    }
    if (!foundLastCompositionStart) {
      // Find previous eCompositionStart of the latest eCompositionStart.
      foundLastCompositionStart = true;
      continue;
    }
    // Remove the messages before the last 2 sets of composition events.
    mReceivedEventMessages.RemoveElementsAt(0, i - 1);
    break;
  }

  if (!numberOfCompositionCommitRequestHandled) {
    // If there is no eCompositionCommitRequestHandled in
    // mReceivedEventMessages, we don't need to store log of
    // RequestIMEToCommmitComposition().
    mRequestIMEToCommitCompositionResults.Clear();
  } else {
    // We need to keep all reason of eCompositionCommitRequestHandled, which
    // is sent when mRequestIMEToCommitComposition() returns true.
    // So, we can discard older log than the first
    // eCompositionCommitRequestHandled in mReceivedEventMessages.
    for (size_t i = mRequestIMEToCommitCompositionResults.Length();
         i > 1; i--) {
      if (mRequestIMEToCommitCompositionResults[i - 1] ==
            RequestIMEToCommitCompositionResult::eReceivedAfterTabParentBlur ||
          mRequestIMEToCommitCompositionResults[i - 1] ==
            RequestIMEToCommitCompositionResult::eHandledSynchronously) {
        --numberOfCompositionCommitRequestHandled;
        if (!numberOfCompositionCommitRequestHandled) {
          mRequestIMEToCommitCompositionResults.RemoveElementsAt(0, i - 1);
          break;
        }
      }
    }
  }
}

void
ContentCacheInParent::AppendEventMessageLog(nsACString& aLog) const
{
  aLog.AppendLiteral("Dispatched Event Message Log:\n");
  for (EventMessage message : mDispatchedEventMessages) {
    aLog.AppendLiteral("  ");
    aLog.Append(ToChar(message));
    aLog.AppendLiteral("\n");
  }
  aLog.AppendLiteral("\nReceived Event Message Log:\n");
  for (EventMessage message : mReceivedEventMessages) {
    aLog.AppendLiteral("  ");
    aLog.Append(ToChar(message));
    aLog.AppendLiteral("\n");
  }
  aLog.AppendLiteral("\nResult of RequestIMEToCommitComposition():\n");
  for (RequestIMEToCommitCompositionResult result :
         mRequestIMEToCommitCompositionResults) {
    aLog.AppendLiteral("  ");
    aLog.Append(ToReadableText(result));
    aLog.AppendLiteral("\n");
  }
  aLog.AppendLiteral("\n");
}

#endif // #if MOZ_DIAGNOSTIC_ASSERT_ENABLED

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

LayoutDeviceIntRect
ContentCache::TextRectArray::GetUnionRectAsFarAsPossible(
                               uint32_t aOffset,
                               uint32_t aLength,
                               bool aRoundToExistingOffset) const
{
  LayoutDeviceIntRect rect;
  if (!HasRects() ||
      (!aRoundToExistingOffset && !IsOverlappingWith(aOffset, aLength))) {
    return rect;
  }
  uint32_t startOffset = std::max(aOffset, mStart);
  if (aRoundToExistingOffset && startOffset >= EndOffset()) {
    startOffset = EndOffset() - 1;
  }
  uint32_t endOffset = std::min(aOffset + aLength, EndOffset());
  if (aRoundToExistingOffset && endOffset < mStart + 1) {
    endOffset = mStart + 1;
  }
  if (NS_WARN_IF(endOffset < startOffset)) {
    return rect;
  }
  for (uint32_t i = 0; i < endOffset - startOffset; i++) {
    rect = rect.Union(mRects[startOffset - mStart + i]);
  }
  return rect;
}

} // namespace mozilla
