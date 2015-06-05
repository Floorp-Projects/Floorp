/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ContentCache.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/Logging.h"
#include "mozilla/TextComposition.h"
#include "mozilla/TextEvents.h"
#include "nsIWidget.h"
#include "nsRefPtr.h"

namespace mozilla {

using namespace widget;

static const char*
GetBoolName(bool aBool)
{
  return aBool ? "true" : "false";
}

static const char*
GetEventMessageName(uint32_t aMessage)
{
  switch (aMessage) {
    case NS_COMPOSITION_START:
      return "NS_COMPOSITION_START";
    case NS_COMPOSITION_END:
      return "NS_COMPOSITION_END";
    case NS_COMPOSITION_UPDATE:
      return "NS_COMPOSITION_UPDATE";
    case NS_COMPOSITION_CHANGE:
      return "NS_COMPOSITION_CHANGE";
    case NS_COMPOSITION_COMMIT_AS_IS:
      return "NS_COMPOSITION_COMMIT_AS_IS";
    case NS_COMPOSITION_COMMIT:
      return "NS_COMPOSITION_COMMIT";
    default:
      return "unacceptable event message";
  }
}

static const char*
GetNotificationName(const IMENotification* aNotification)
{
  if (!aNotification) {
    return "Not notification";
  }
  switch (aNotification->mMessage) {
    case NOTIFY_IME_OF_FOCUS:
      return "NOTIFY_IME_OF_FOCUS";
    case NOTIFY_IME_OF_BLUR:
      return "NOTIFY_IME_OF_BLUR";
    case NOTIFY_IME_OF_SELECTION_CHANGE:
      return "NOTIFY_IME_OF_SELECTION_CHANGE";
    case NOTIFY_IME_OF_TEXT_CHANGE:
      return "NOTIFY_IME_OF_TEXT_CHANGE";
    case NOTIFY_IME_OF_COMPOSITION_UPDATE:
      return "NOTIFY_IME_OF_COMPOSITION_UPDATE";
    case NOTIFY_IME_OF_POSITION_CHANGE:
      return "NOTIFY_IME_OF_POSITION_CHANGE";
    case NOTIFY_IME_OF_MOUSE_BUTTON_EVENT:
      return "NOTIFY_IME_OF_MOUSE_BUTTON_EVENT";
    case REQUEST_TO_COMMIT_COMPOSITION:
      return "REQUEST_TO_COMMIT_COMPOSITION";
    case REQUEST_TO_CANCEL_COMPOSITION:
      return "REQUEST_TO_CANCEL_COMPOSITION";
    default:
      return "Unsupported notification";
  }
}

class GetRectText : public nsAutoCString
{
public:
  explicit GetRectText(const LayoutDeviceIntRect& aRect)
  {
    Assign("{ x=");
    AppendInt(aRect.x);
    Append(", y=");
    AppendInt(aRect.y);
    Append(", width=");
    AppendInt(aRect.width);
    Append(", height=");
    AppendInt(aRect.height);
    Append(" }");
  }
  virtual ~GetRectText() {}
};

class GetWritingModeName : public nsAutoCString
{
public:
  explicit GetWritingModeName(const WritingMode& aWritingMode)
  {
    if (!aWritingMode.IsVertical()) {
      Assign("Horizontal");
      return;
    }
    if (aWritingMode.IsVerticalLR()) {
      Assign("Vertical (LTR)");
      return;
    }
    Assign("Vertical (RTL)");
  }
  virtual ~GetWritingModeName() {}
};

/*****************************************************************************
 * mozilla::ContentCache
 *****************************************************************************/

PRLogModuleInfo* sContentCacheLog = nullptr;

ContentCache::ContentCache()
  : mCompositionStart(UINT32_MAX)
  , mCompositionEventsDuringRequest(0)
  , mIsComposing(false)
  , mRequestedToCommitOrCancelComposition(false)
  , mIsChrome(XRE_GetProcessType() == GeckoProcessType_Default)
{
  if (!sContentCacheLog) {
    sContentCacheLog = PR_NewLogModule("ContentCacheWidgets");
  }
}

void
ContentCache::AssignContent(const ContentCache& aOther,
                            const IMENotification* aNotification)
{
  mText = aOther.mText;
  mSelection = aOther.mSelection;
  mCaret = aOther.mCaret;
  mTextRectArray = aOther.mTextRectArray;
  mEditorRect = aOther.mEditorRect;

  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("ContentCache: 0x%p (mIsChrome=%s) AssignContent(aNotification=%s), "
     "Succeeded, mText.Length()=%u, mSelection={ mAnchor=%u, mFocus=%u, "
     " mWritingMode=%s }, mCaret={ mOffset=%u, mRect=%s }, "
     "mTextRectArray={ mStart=%u, mRects.Length()=%u }, mEditorRect=%s",
     this, GetBoolName(mIsChrome), GetNotificationName(aNotification),
     mText.Length(), mSelection.mAnchor, mSelection.mFocus,
     GetWritingModeName(mSelection.mWritingMode).get(), mCaret.mOffset,
     GetRectText(mCaret.mRect).get(), mTextRectArray.mStart,
     mTextRectArray.mRects.Length(), GetRectText(mEditorRect).get()));
}

void
ContentCache::Clear()
{
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("ContentCache: 0x%p (mIsChrome=%s) Clear()",
     this, GetBoolName(mIsChrome)));

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
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
        ("ContentCache: 0x%p (mIsChrome=%s) HandleQueryContentEvent("
         "aEvent={ message=NS_QUERY_SELECTED_TEXT }, aWidget=0x%p)",
         this, GetBoolName(mIsChrome), aWidget));
      aEvent.mReply.mOffset = mSelection.StartOffset();
      if (mSelection.Collapsed()) {
        aEvent.mReply.mString.Truncate(0);
      } else {
        if (NS_WARN_IF(mSelection.EndOffset() > mText.Length())) {
          MOZ_LOG(sContentCacheLog, LogLevel::Error,
            ("ContentCache: 0x%p (mIsChrome=%s) HandleQueryContentEvent(), "
             "FAILED because mSelection.EndOffset()=%u is larger than "
             "mText.Length()=%u",
             this, GetBoolName(mIsChrome), mSelection.EndOffset(),
             mText.Length()));
          return false;
        }
        aEvent.mReply.mString =
          Substring(mText, aEvent.mReply.mOffset, mSelection.Length());
      }
      aEvent.mReply.mReversed = mSelection.Reversed();
      aEvent.mReply.mHasSelection = true;
      aEvent.mReply.mWritingMode = mSelection.mWritingMode;
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
        ("ContentCache: 0x%p (mIsChrome=%s) HandleQueryContentEvent(), "
         "Succeeded, aEvent={ mReply={ mOffset=%u, mString=\"%s\", "
         "mReversed=%s, mHasSelection=%s, mWritingMode=%s } }",
         this, GetBoolName(mIsChrome), aEvent.mReply.mOffset,
         NS_ConvertUTF16toUTF8(aEvent.mReply.mString).get(),
         GetBoolName(aEvent.mReply.mReversed),
         GetBoolName(aEvent.mReply.mHasSelection),
         GetWritingModeName(aEvent.mReply.mWritingMode).get()));
      break;
    case NS_QUERY_TEXT_CONTENT: {
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
        ("ContentCache: 0x%p (mIsChrome=%s) HandleQueryContentEvent("
         "aEvent={ message=NS_QUERY_TEXT_CONTENT, mInput={ mOffset=%u, "
         "mLength=%u } }, aWidget=0x%p), mText.Length()=%u",
         this, GetBoolName(mIsChrome), aEvent.mInput.mOffset,
         aEvent.mInput.mLength, aWidget, mText.Length()));
      uint32_t inputOffset = aEvent.mInput.mOffset;
      uint32_t inputEndOffset =
        std::min(aEvent.mInput.EndOffset(), mText.Length());
      if (NS_WARN_IF(inputEndOffset < inputOffset)) {
        MOZ_LOG(sContentCacheLog, LogLevel::Error,
          ("ContentCache: 0x%p (mIsChrome=%s) HandleQueryContentEvent(), "
           "FAILED because inputOffset=%u is larger than inputEndOffset=%u",
           this, GetBoolName(mIsChrome), inputOffset, inputEndOffset));
        return false;
      }
      aEvent.mReply.mOffset = inputOffset;
      aEvent.mReply.mString =
        Substring(mText, inputOffset, inputEndOffset - inputOffset);
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
        ("ContentCache: 0x%p (mIsChrome=%s) HandleQueryContentEvent(), "
         "Succeeded, aEvent={ mReply={ mOffset=%u, mString.Length()=%u } }",
         this, GetBoolName(mIsChrome), aEvent.mReply.mOffset,
         aEvent.mReply.mString.Length()));
      break;
    }
    case NS_QUERY_TEXT_RECT:
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
        ("ContentCache: 0x%p (mIsChrome=%s) HandleQueryContentEvent("
         "aEvent={ message=NS_QUERY_TEXT_RECT, mInput={ mOffset=%u, "
         "mLength=%u } }, aWidget=0x%p), mText.Length()=%u",
         this, GetBoolName(mIsChrome), aEvent.mInput.mOffset,
         aEvent.mInput.mLength, aWidget, mText.Length()));
      if (NS_WARN_IF(!GetUnionTextRects(aEvent.mInput.mOffset,
                                        aEvent.mInput.mLength,
                                        aEvent.mReply.mRect))) {
        // XXX We don't have cache for this request.
        MOZ_LOG(sContentCacheLog, LogLevel::Error,
          ("ContentCache: 0x%p (mIsChrome=%s) HandleQueryContentEvent(), "
           "FAILED to get union rect",
           this, GetBoolName(mIsChrome)));
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
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
        ("ContentCache: 0x%p (mIsChrome=%s) HandleQueryContentEvent(), "
         "Succeeded, aEvent={ mReply={ mOffset=%u, mString=\"%s\", "
         "mWritingMode=%s, mRect=%s } }",
         this, GetBoolName(mIsChrome), aEvent.mReply.mOffset,
         NS_ConvertUTF16toUTF8(aEvent.mReply.mString).get(),
         GetWritingModeName(aEvent.mReply.mWritingMode).get(),
         GetRectText(aEvent.mReply.mRect).get()));
      break;
    case NS_QUERY_CARET_RECT:
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
        ("ContentCache: 0x%p (mIsChrome=%s) HandleQueryContentEvent("
         "aEvent={ message=NS_QUERY_CARET_RECT, mInput={ mOffset=%u } }, "
         "aWidget=0x%p), mText.Length()=%u",
         this, GetBoolName(mIsChrome), aEvent.mInput.mOffset, aWidget,
         mText.Length()));
      if (NS_WARN_IF(!GetCaretRect(aEvent.mInput.mOffset,
                                   aEvent.mReply.mRect))) {
        // XXX If the input offset is in the range of cached text rects,
        //     we can guess the caret rect.
        MOZ_LOG(sContentCacheLog, LogLevel::Error,
          ("ContentCache: 0x%p (mIsChrome=%s) HandleQueryContentEvent(), "
           "FAILED to get caret rect",
           this, GetBoolName(mIsChrome)));
        return false;
      }
      aEvent.mReply.mOffset = aEvent.mInput.mOffset;
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
        ("ContentCache: 0x%p (mIsChrome=%s) HandleQueryContentEvent(), "
         "Succeeded, aEvent={ mReply={ mOffset=%u, mRect=%s } }",
         this, GetBoolName(mIsChrome), aEvent.mReply.mOffset,
         GetRectText(aEvent.mReply.mRect).get()));
      break;
    case NS_QUERY_EDITOR_RECT:
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
        ("ContentCache: 0x%p (mIsChrome=%s) HandleQueryContentEvent("
         "aEvent={ message=NS_QUERY_EDITOR_RECT }, aWidget=0x%p)",
         this, GetBoolName(mIsChrome), aWidget));
      aEvent.mReply.mRect = mEditorRect;
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
        ("ContentCache: 0x%p (mIsChrome=%s) HandleQueryContentEvent(), "
         "Succeeded, aEvent={ mReply={ mRect=%s } }",
         this, GetBoolName(mIsChrome), GetRectText(aEvent.mReply.mRect).get()));
      break;
  }
  aEvent.mSucceeded = true;
  return true;
}

bool
ContentCache::CacheAll(nsIWidget* aWidget,
                       const IMENotification* aNotification)
{
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("ContentCache: 0x%p (mIsChrome=%s) CacheAll(aWidget=0x%p, "
     "aNotification=%s)",
     this, GetBoolName(mIsChrome), aWidget,
     GetNotificationName(aNotification)));

  // CacheAll() must be called in content process.
  if (NS_WARN_IF(mIsChrome)) {
    return false;
  }

  if (NS_WARN_IF(!CacheText(aWidget, aNotification)) ||
      NS_WARN_IF(!CacheSelection(aWidget, aNotification)) ||
      NS_WARN_IF(!CacheTextRects(aWidget, aNotification)) ||
      NS_WARN_IF(!CacheEditorRect(aWidget, aNotification))) {
    return false;
  }
  return true;
}

bool
ContentCache::CacheSelection(nsIWidget* aWidget,
                             const IMENotification* aNotification)
{
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("ContentCache: 0x%p (mIsChrome=%s) CacheSelection(aWidget=0x%p, "
     "aNotification=%s)",
     this, GetBoolName(mIsChrome), aWidget,
     GetNotificationName(aNotification)));

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
    MOZ_LOG(sContentCacheLog, LogLevel::Error,
      ("ContentCache: 0x%p (mIsChrome=%s) CacheSelection(), FAILED, "
       "couldn't retrieve the selected text",
       this, GetBoolName(mIsChrome)));
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
    MOZ_LOG(sContentCacheLog, LogLevel::Error,
      ("ContentCache: 0x%p (mIsChrome=%s) CacheSelection(), FAILED, "
       "couldn't retrieve the caret rect at offset=%u",
       this, GetBoolName(mIsChrome), mCaret.mOffset));
    mCaret.Clear();
    return false;
  }
  mCaret.mRect = caretRect.mReply.mRect;
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("ContentCache: 0x%p (mIsChrome=%s) CacheSelection(), Succeeded, "
     "mSelection={ mAnchor=%u, mFocus=%u, mWritingMode=%s }, "
     "mCaret={ mOffset=%u, mRect=%s }",
     this, GetBoolName(mIsChrome), mSelection.mAnchor, mSelection.mFocus,
     GetWritingModeName(mSelection.mWritingMode).get(), mCaret.mOffset,
     GetRectText(mCaret.mRect).get()));
  return true;
}

bool
ContentCache::CacheEditorRect(nsIWidget* aWidget,
                              const IMENotification* aNotification)
{
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("ContentCache: 0x%p (mIsChrome=%s) CacheEditorRect(aWidget=0x%p, "
     "aNotification=%s)",
     this, GetBoolName(mIsChrome), aWidget,
     GetNotificationName(aNotification)));

  // CacheEditorRect() must be called in content process.
  if (NS_WARN_IF(mIsChrome)) {
    return false;
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetQueryContentEvent editorRectEvent(true, NS_QUERY_EDITOR_RECT, aWidget);
  aWidget->DispatchEvent(&editorRectEvent, status);
  if (NS_WARN_IF(!editorRectEvent.mSucceeded)) {
    MOZ_LOG(sContentCacheLog, LogLevel::Error,
      ("ContentCache: 0x%p (mIsChrome=%s) CacheEditorRect(), FAILED, "
       "couldn't retrieve the editor rect",
       this, GetBoolName(mIsChrome)));
    return false;
  }
  mEditorRect = editorRectEvent.mReply.mRect;
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("ContentCache: 0x%p (mIsChrome=%s) CacheEditorRect(), Succeeded, "
     "mEditorRect=%s",
     this, GetBoolName(mIsChrome), GetRectText(mEditorRect).get()));
  return true;
}

bool
ContentCache::CacheText(nsIWidget* aWidget,
                        const IMENotification* aNotification)
{
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("ContentCache: 0x%p (mIsChrome=%s) CacheText(aWidget=0x%p, "
     "aNotification=%s)",
     this, GetBoolName(mIsChrome), aWidget,
     GetNotificationName(aNotification)));

  // CacheText() must be called in content process.
  if (NS_WARN_IF(mIsChrome)) {
    return false;
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetQueryContentEvent queryText(true, NS_QUERY_TEXT_CONTENT, aWidget);
  queryText.InitForQueryTextContent(0, UINT32_MAX);
  aWidget->DispatchEvent(&queryText, status);
  if (NS_WARN_IF(!queryText.mSucceeded)) {
    MOZ_LOG(sContentCacheLog, LogLevel::Error,
      ("ContentCache: 0x%p (mIsChrome=%s) CacheText(), FAILED, "
       "couldn't retrieve whole text",
       this, GetBoolName(mIsChrome)));
    mText.Truncate();
    return false;
  }
  mText = queryText.mReply.mString;
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("ContentCache: 0x%p (mIsChrome=%s) CacheText(), Succeeded, "
     "mText.Length()=%u",
     this, GetBoolName(mIsChrome), mText.Length()));
  return true;
}

bool
ContentCache::CacheTextRects(nsIWidget* aWidget,
                             const IMENotification* aNotification)
{
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("ContentCache: 0x%p (mIsChrome=%s) CacheTextRects(aWidget=0x%p, "
     "aNotification=%s)",
     this, GetBoolName(mIsChrome), aWidget,
     GetNotificationName(aNotification)));

  // CacheTextRects() must be called in content process.
  if (NS_WARN_IF(mIsChrome)) {
    return false;
  }

  mTextRectArray.Clear();

  nsRefPtr<TextComposition> textComposition =
    IMEStateManager::GetTextCompositionFor(aWidget);
  if (NS_WARN_IF(!textComposition)) {
    MOZ_LOG(sContentCacheLog, LogLevel::Info,
      ("ContentCache: 0x%p (mIsChrome=%s) CacheTextRects(), not caching "
       "text rects because there is no composition string",
       this, GetBoolName(mIsChrome), aWidget));
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
      MOZ_LOG(sContentCacheLog, LogLevel::Error,
        ("ContentCache: 0x%p (mIsChrome=%s) CacheTextRects(), FAILED, "
         "couldn't retrieve text rect at offset=%u",
         this, GetBoolName(mIsChrome), i));
      mTextRectArray.Clear();
      return false;
    }
    mTextRectArray.mRects.AppendElement(textRect.mReply.mRect);
  }
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("ContentCache: 0x%p (mIsChrome=%s) CacheTextRects(), Succeeded, "
     "mTextRectArray={ mStart=%u, mRects.Length()=%u }, mText.Length()=%u",
     this, GetBoolName(mIsChrome), mTextRectArray.mStart,
     mTextRectArray.mRects.Length(), mText.Length()));
  return true;
}

void
ContentCache::SetSelection(uint32_t aStartOffset,
                           uint32_t aLength,
                           bool aReversed,
                           const WritingMode& aWritingMode)
{
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("ContentCache: 0x%p (mIsChrome=%s) SetSelection(aStartOffset=%u, "
     "aLength=%u, aReversed=%s, aWritingMode=%s), mText.Length()=%u",
     this, GetBoolName(mIsChrome), aStartOffset, aLength,
     GetBoolName(aReversed), GetWritingModeName(aWritingMode).get(),
     mText.Length()));

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
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("ContentCache: 0x%p (mIsChrome=%s) SetSelection(aAnchorOffset=%u, "
     "aFocusOffset=%u, aWritingMode=%s), mText.Length()=%u",
     this, GetBoolName(mIsChrome), aAnchorOffset, aFocusOffset,
     GetWritingModeName(aWritingMode).get(), mText.Length()));

  mSelection.mAnchor = aAnchorOffset;
  mSelection.mFocus = aFocusOffset;
  mSelection.mWritingMode = aWritingMode;
}

bool
ContentCache::GetTextRect(uint32_t aOffset,
                          LayoutDeviceIntRect& aTextRect) const
{
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("ContentCache: 0x%p (mIsChrome=%s) GetTextRect(aOffset=%u), "
     "mTextRectArray={ mStart=%u, mRects.Length()=%u }",
     this, GetBoolName(mIsChrome), aOffset, mTextRectArray.mStart,
     mTextRectArray.mRects.Length()));

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
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("ContentCache: 0x%p (mIsChrome=%s) GetUnionTextRects(aOffset=%u, "
     "aLength=%u), mTextRectArray={ mStart=%u, mRects.Length()=%u }",
     this, GetBoolName(mIsChrome), aOffset, aLength,
     mTextRectArray.mStart, mTextRectArray.mRects.Length()));

  if (NS_WARN_IF(!mTextRectArray.InRange(aOffset, aLength))) {
    aUnionTextRect.SetEmpty();
    return false;
  }
  aUnionTextRect = mTextRectArray.GetUnionRect(aOffset, aLength);
  return true;
}

bool
ContentCache::GetCaretRect(uint32_t aOffset,
                           LayoutDeviceIntRect& aCaretRect) const
{
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("ContentCache: 0x%p (mIsChrome=%s) GetCaretRect(aOffset=%u), "
     "mCaret={ mOffset=%u, mRect=%s }",
     this, GetBoolName(mIsChrome), aOffset, mCaret.mOffset,
     GetRectText(mCaret.mRect).get()));

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
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("ContentCache: 0x%p (mIsChrome=%s) OnCompositionEvent(aEvent={ "
     "message=%s, mData=\"%s\" (Length()=%u), mRanges->Length()=%u }), "
     "mIsComposing=%s, mRequestedToCommitOrCancelComposition=%s",
     this, GetBoolName(mIsChrome), GetEventMessageName(aEvent.message),
     NS_ConvertUTF16toUTF8(aEvent.mData).get(), aEvent.mData.Length(),
     aEvent.mRanges ? aEvent.mRanges->Length() : 0, GetBoolName(mIsComposing),
     GetBoolName(mRequestedToCommitOrCancelComposition)));

  if (!aEvent.CausesDOMTextEvent()) {
    MOZ_ASSERT(aEvent.message == NS_COMPOSITION_START);
    mIsComposing = !aEvent.CausesDOMCompositionEndEvent();
    mCompositionStart = mSelection.StartOffset();
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
    mCompositionStart = mSelection.StartOffset();
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
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("ContentCache: 0x%p (mIsChrome=%s) RequestToCommitComposition(aWidget=%p, "
     "aCancel=%s), mIsComposing=%s, mRequestedToCommitOrCancelComposition=%s, "
     "mCompositionEventsDuringRequest=%u",
     this, GetBoolName(mIsChrome), aWidget, GetBoolName(aCancel),
     GetBoolName(mIsComposing),
     GetBoolName(mRequestedToCommitOrCancelComposition),
     mCompositionEventsDuringRequest));

  mRequestedToCommitOrCancelComposition = true;
  mCompositionEventsDuringRequest = 0;

  aWidget->NotifyIME(IMENotification(aCancel ? REQUEST_TO_CANCEL_COMPOSITION :
                                               REQUEST_TO_COMMIT_COMPOSITION));

  mRequestedToCommitOrCancelComposition = false;
  aLastString = mCommitStringByRequest;
  mCommitStringByRequest.Truncate(0);
  return mCompositionEventsDuringRequest;
}

void
ContentCache::InitNotification(IMENotification& aNotification) const
{
  if (NS_WARN_IF(aNotification.mMessage != NOTIFY_IME_OF_SELECTION_CHANGE)) {
    return;
  }
  aNotification.mSelectionChangeData.mOffset = mSelection.StartOffset();
  aNotification.mSelectionChangeData.mLength = mSelection.Length();
  aNotification.mSelectionChangeData.mReversed = mSelection.Reversed();
  aNotification.mSelectionChangeData.SetWritingMode(mSelection.mWritingMode);
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
