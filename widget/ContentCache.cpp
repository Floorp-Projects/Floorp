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
#include "mozilla/RefPtr.h"

namespace mozilla {

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
{
  if (!sContentCacheLog) {
    sContentCacheLog = PR_NewLogModule("ContentCacheWidgets");
  }
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
    ("ContentCacheInChild: 0x%p Clear()", this));

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
    ("ContentCacheInChild: 0x%p CacheAll(aWidget=0x%p, "
     "aNotification=%s)",
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
    ("ContentCacheInChild: 0x%p CacheSelection(aWidget=0x%p, "
     "aNotification=%s)",
     this, aWidget, GetNotificationName(aNotification)));

  mCaret.Clear();
  mSelection.Clear();

  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetQueryContentEvent selection(true, eQuerySelectedText, aWidget);
  aWidget->DispatchEvent(&selection, status);
  if (NS_WARN_IF(!selection.mSucceeded)) {
    MOZ_LOG(sContentCacheLog, LogLevel::Error,
      ("ContentCache: 0x%p CacheSelection(), FAILED, "
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
    ("ContentCacheInChild: 0x%p CacheCaret(aWidget=0x%p, "
     "aNotification=%s)",
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
      ("ContentCacheInChild: 0x%p CacheCaret(), FAILED, "
       "couldn't retrieve the caret rect at offset=%u",
       this, mCaret.mOffset));
    mCaret.Clear();
    return false;
  }
  mCaret.mRect = caretRect.mReply.mRect;
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("ContentCacheInChild: 0x%p CacheCaret(), Succeeded, "
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
    ("ContentCacheInChild: 0x%p CacheEditorRect(aWidget=0x%p, "
     "aNotification=%s)",
     this, aWidget, GetNotificationName(aNotification)));

  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetQueryContentEvent editorRectEvent(true, eQueryEditorRect, aWidget);
  aWidget->DispatchEvent(&editorRectEvent, status);
  if (NS_WARN_IF(!editorRectEvent.mSucceeded)) {
    MOZ_LOG(sContentCacheLog, LogLevel::Error,
      ("ContentCacheInChild: 0x%p CacheEditorRect(), FAILED, "
       "couldn't retrieve the editor rect", this));
    return false;
  }
  mEditorRect = editorRectEvent.mReply.mRect;
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("ContentCacheInChild: 0x%p CacheEditorRect(), Succeeded, "
     "mEditorRect=%s", this, GetRectText(mEditorRect).get()));
  return true;
}

bool
ContentCacheInChild::CacheText(nsIWidget* aWidget,
                               const IMENotification* aNotification)
{
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("ContentCacheInChild: 0x%p CacheText(aWidget=0x%p, "
     "aNotification=%s)",
     this, aWidget, GetNotificationName(aNotification)));

  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetQueryContentEvent queryText(true, eQueryTextContent, aWidget);
  queryText.InitForQueryTextContent(0, UINT32_MAX);
  aWidget->DispatchEvent(&queryText, status);
  if (NS_WARN_IF(!queryText.mSucceeded)) {
    MOZ_LOG(sContentCacheLog, LogLevel::Error,
      ("ContentCacheInChild: 0x%p CacheText(), FAILED, "
       "couldn't retrieve whole text", this));
    mText.Truncate();
    return false;
  }
  mText = queryText.mReply.mString;
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("ContentCacheInChild: 0x%p CacheText(), Succeeded, "
     "mText.Length()=%u", this, mText.Length()));

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
  if (NS_WARN_IF(!aCharRect.height)) {
    aCharRect.height = 1;
  }
  if (NS_WARN_IF(!aCharRect.width)) {
    aCharRect.width = 1;
  }
  return true;
}

bool
ContentCacheInChild::CacheTextRects(nsIWidget* aWidget,
                                    const IMENotification* aNotification)
{
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("ContentCacheInChild: 0x%p CacheTextRects(aWidget=0x%p, "
     "aNotification=%s), mCaret={ mOffset=%u, IsValid()=%s }",
     this, aWidget, GetNotificationName(aNotification), mCaret.mOffset,
     GetBoolName(mCaret.IsValid())));

  mTextRectArray.Clear();
  mSelection.mAnchorCharRect.SetEmpty();
  mSelection.mFocusCharRect.SetEmpty();
  mSelection.mRect.SetEmpty();
  mFirstCharRect.SetEmpty();

  if (NS_WARN_IF(!mSelection.IsValid())) {
    return false;
  }

  // Retrieve text rects in composition string if there is.
  RefPtr<TextComposition> textComposition =
    IMEStateManager::GetTextCompositionFor(aWidget);
  if (textComposition) {
    // Note that TextComposition::String() may not be modified here because
    // it's modified after all edit action listeners are performed but this
    // is called while some of them are performed.
    uint32_t length = textComposition->LastData().Length();
    mTextRectArray.mRects.SetCapacity(length);
    mTextRectArray.mStart = textComposition->NativeOffsetOfStartComposition();
    uint32_t endOffset = mTextRectArray.mStart + length;
    for (uint32_t i = mTextRectArray.mStart; i < endOffset; i++) {
      LayoutDeviceIntRect charRect;
      if (NS_WARN_IF(!QueryCharRect(aWidget, i, charRect))) {
        MOZ_LOG(sContentCacheLog, LogLevel::Error,
          ("ContentCacheInChild: 0x%p CacheTextRects(), FAILED, "
           "couldn't retrieve text rect at offset=%u", this, i));
        mTextRectArray.Clear();
        return false;
      }
      mTextRectArray.mRects.AppendElement(charRect);
    }
  }

  if (mTextRectArray.InRange(mSelection.mAnchor)) {
    mSelection.mAnchorCharRect = mTextRectArray.GetRect(mSelection.mAnchor);
  } else {
    LayoutDeviceIntRect charRect;
    if (NS_WARN_IF(!QueryCharRect(aWidget, mSelection.mAnchor, charRect))) {
      MOZ_LOG(sContentCacheLog, LogLevel::Error,
        ("ContentCacheInChild: 0x%p CacheTextRects(), FAILED, "
         "couldn't retrieve text rect at anchor of selection (%u)",
         this, mSelection.mAnchor));
    }
    mSelection.mAnchorCharRect = charRect;
  }

  if (mSelection.Collapsed()) {
    mSelection.mFocusCharRect = mSelection.mAnchorCharRect;
  } else if (mTextRectArray.InRange(mSelection.mFocus)) {
    mSelection.mFocusCharRect = mTextRectArray.GetRect(mSelection.mFocus);
  } else {
    LayoutDeviceIntRect charRect;
    if (NS_WARN_IF(!QueryCharRect(aWidget, mSelection.mFocus, charRect))) {
      MOZ_LOG(sContentCacheLog, LogLevel::Error,
        ("ContentCacheInChild: 0x%p CacheTextRects(), FAILED, "
         "couldn't retrieve text rect at focus of selection (%u)",
         this, mSelection.mFocus));
    }
    mSelection.mFocusCharRect = charRect;
  }

  if (!mSelection.Collapsed()) {
    nsEventStatus status = nsEventStatus_eIgnore;
    WidgetQueryContentEvent textRect(true, eQueryTextRect, aWidget);
    textRect.InitForQueryTextRect(mSelection.StartOffset(),
                                  mSelection.Length());
    aWidget->DispatchEvent(&textRect, status);
    if (NS_WARN_IF(!textRect.mSucceeded)) {
      MOZ_LOG(sContentCacheLog, LogLevel::Error,
        ("ContentCacheInChild: 0x%p CacheTextRects(), FAILED, "
         "couldn't retrieve text rect of whole selected text", this));
    } else {
      mSelection.mRect = textRect.mReply.mRect;
    }
  }

  if (!mSelection.mFocus) {
    mFirstCharRect = mSelection.mFocusCharRect;
  } else if (!mSelection.mAnchor) {
    mFirstCharRect = mSelection.mAnchorCharRect;
  } else if (mTextRectArray.InRange(0)) {
    mFirstCharRect = mTextRectArray.GetRect(0);
  } else {
    LayoutDeviceIntRect charRect;
    if (NS_WARN_IF(!QueryCharRect(aWidget, 0, charRect))) {
      MOZ_LOG(sContentCacheLog, LogLevel::Error,
        ("ContentCacheInChild: 0x%p CacheTextRects(), FAILED, "
         "couldn't retrieve first char rect", this));
    } else {
      mFirstCharRect = charRect;
    }
  }

  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("ContentCacheInChild: 0x%p CacheTextRects(), Succeeded, "
     "mText.Length()=%u, mTextRectArray={ mStart=%u, mRects.Length()=%u }, "
     "mSelection={ mAnchor=%u, mAnchorCharRect=%s, mFocus=%u, "
     "mFocusCharRect=%s, mRect=%s }, mFirstCharRect=%s",
     this, mText.Length(), mTextRectArray.mStart,
     mTextRectArray.mRects.Length(), mSelection.mAnchor,
     GetRectText(mSelection.mAnchorCharRect).get(), mSelection.mFocus,
     GetRectText(mSelection.mFocusCharRect).get(),
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
    ("ContentCacheInChild: 0x%p SetSelection(aStartOffset=%u, "
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
  NS_WARN_IF(!CacheTextRects(aWidget));
}

/*****************************************************************************
 * mozilla::ContentCacheInParent
 *****************************************************************************/

ContentCacheInParent::ContentCacheInParent()
  : ContentCache()
  , mCommitStringByRequest(nullptr)
  , mCompositionStart(UINT32_MAX)
  , mPendingEventsNeedingAck(0)
  , mIsComposing(false)
{
}

void
ContentCacheInParent::AssignContent(const ContentCache& aOther,
                                    const IMENotification* aNotification)
{
  mText = aOther.mText;
  mSelection = aOther.mSelection;
  mFirstCharRect = aOther.mFirstCharRect;
  mCaret = aOther.mCaret;
  mTextRectArray = aOther.mTextRectArray;
  mEditorRect = aOther.mEditorRect;

  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("ContentCacheInParent: 0x%p AssignContent(aNotification=%s), "
     "Succeeded, mText.Length()=%u, mSelection={ mAnchor=%u, mFocus=%u, "
     "mWritingMode=%s, mAnchorCharRect=%s, mFocusCharRect=%s, mRect=%s }, "
     "mFirstCharRect=%s, mCaret={ mOffset=%u, mRect=%s }, mTextRectArray={ "
     "mStart=%u, mRects.Length()=%u }, mEditorRect=%s",
     this, GetNotificationName(aNotification),
     mText.Length(), mSelection.mAnchor, mSelection.mFocus,
     GetWritingModeName(mSelection.mWritingMode).get(),
     GetRectText(mSelection.mAnchorCharRect).get(),
     GetRectText(mSelection.mFocusCharRect).get(),
     GetRectText(mSelection.mRect).get(), GetRectText(mFirstCharRect).get(),
     mCaret.mOffset, GetRectText(mCaret.mRect).get(), mTextRectArray.mStart,
     mTextRectArray.mRects.Length(), GetRectText(mEditorRect).get()));
}

bool
ContentCacheInParent::HandleQueryContentEvent(WidgetQueryContentEvent& aEvent,
                                              nsIWidget* aWidget) const
{
  MOZ_ASSERT(aWidget);

  aEvent.mSucceeded = false;
  aEvent.mReply.mFocusedWidget = aWidget;

  switch (aEvent.mMessage) {
    case eQuerySelectedText:
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
        ("ContentCacheInParent: 0x%p HandleQueryContentEvent("
         "aEvent={ mMessage=eQuerySelectedText }, aWidget=0x%p)",
         this, aWidget));
      if (NS_WARN_IF(!IsSelectionValid())) {
        // If content cache hasn't been initialized properly, make the query
        // failed.
        MOZ_LOG(sContentCacheLog, LogLevel::Error,
          ("ContentCacheInParent: 0x%p HandleQueryContentEvent(), "
           "FAILED because mSelection is not valid", this));
        return true;
      }
      aEvent.mReply.mOffset = mSelection.StartOffset();
      if (mSelection.Collapsed()) {
        aEvent.mReply.mString.Truncate(0);
      } else {
        if (NS_WARN_IF(mSelection.EndOffset() > mText.Length())) {
          MOZ_LOG(sContentCacheLog, LogLevel::Error,
            ("ContentCacheInParent: 0x%p HandleQueryContentEvent(), "
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
        ("ContentCacheInParent: 0x%p HandleQueryContentEvent(), "
         "Succeeded, aEvent={ mReply={ mOffset=%u, mString=\"%s\", "
         "mReversed=%s, mHasSelection=%s, mWritingMode=%s } }",
         this, aEvent.mReply.mOffset,
         NS_ConvertUTF16toUTF8(aEvent.mReply.mString).get(),
         GetBoolName(aEvent.mReply.mReversed),
         GetBoolName(aEvent.mReply.mHasSelection),
         GetWritingModeName(aEvent.mReply.mWritingMode).get()));
      break;
    case eQueryTextContent: {
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
        ("ContentCacheInParent: 0x%p HandleQueryContentEvent("
         "aEvent={ mMessage=eQueryTextContent, mInput={ mOffset=%u, "
         "mLength=%u } }, aWidget=0x%p), mText.Length()=%u",
         this, aEvent.mInput.mOffset,
         aEvent.mInput.mLength, aWidget, mText.Length()));
      uint32_t inputOffset = aEvent.mInput.mOffset;
      uint32_t inputEndOffset =
        std::min(aEvent.mInput.EndOffset(), mText.Length());
      if (NS_WARN_IF(inputEndOffset < inputOffset)) {
        MOZ_LOG(sContentCacheLog, LogLevel::Error,
          ("ContentCacheInParent: 0x%p HandleQueryContentEvent(), "
           "FAILED because inputOffset=%u is larger than inputEndOffset=%u",
           this, inputOffset, inputEndOffset));
        return false;
      }
      aEvent.mReply.mOffset = inputOffset;
      aEvent.mReply.mString =
        Substring(mText, inputOffset, inputEndOffset - inputOffset);
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
        ("ContentCacheInParent: 0x%p HandleQueryContentEvent(), "
         "Succeeded, aEvent={ mReply={ mOffset=%u, mString.Length()=%u } }",
         this, aEvent.mReply.mOffset, aEvent.mReply.mString.Length()));
      break;
    }
    case eQueryTextRect:
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
        ("ContentCacheInParent: 0x%p HandleQueryContentEvent("
         "aEvent={ mMessage=eQueryTextRect, mInput={ mOffset=%u, "
         "mLength=%u } }, aWidget=0x%p), mText.Length()=%u",
         this, aEvent.mInput.mOffset, aEvent.mInput.mLength, aWidget,
         mText.Length()));
      if (NS_WARN_IF(!IsSelectionValid())) {
        // If content cache hasn't been initialized properly, make the query
        // failed.
        MOZ_LOG(sContentCacheLog, LogLevel::Error,
          ("ContentCacheInParent: 0x%p HandleQueryContentEvent(), "
           "FAILED because mSelection is not valid", this));
        return true;
      }
      if (aEvent.mInput.mLength) {
        if (NS_WARN_IF(!GetUnionTextRects(aEvent.mInput.mOffset,
                                          aEvent.mInput.mLength,
                                          aEvent.mReply.mRect))) {
          // XXX We don't have cache for this request.
          MOZ_LOG(sContentCacheLog, LogLevel::Error,
            ("ContentCacheInParent: 0x%p HandleQueryContentEvent(), "
             "FAILED to get union rect", this));
          return false;
        }
      } else {
        // If the length is 0, we should return caret rect instead.
        if (NS_WARN_IF(!GetCaretRect(aEvent.mInput.mOffset,
                                     aEvent.mReply.mRect))) {
          MOZ_LOG(sContentCacheLog, LogLevel::Error,
            ("ContentCacheInParent: 0x%p HandleQueryContentEvent(), "
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
        ("ContentCacheInParent: 0x%p HandleQueryContentEvent(), "
         "Succeeded, aEvent={ mReply={ mOffset=%u, mString=\"%s\", "
         "mWritingMode=%s, mRect=%s } }",
         this, aEvent.mReply.mOffset,
         NS_ConvertUTF16toUTF8(aEvent.mReply.mString).get(),
         GetWritingModeName(aEvent.mReply.mWritingMode).get(),
         GetRectText(aEvent.mReply.mRect).get()));
      break;
    case eQueryCaretRect:
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
        ("ContentCacheInParent: 0x%p HandleQueryContentEvent("
         "aEvent={ mMessage=eQueryCaretRect, mInput={ mOffset=%u } }, "
         "aWidget=0x%p), mText.Length()=%u",
         this, aEvent.mInput.mOffset, aWidget, mText.Length()));
      if (NS_WARN_IF(!IsSelectionValid())) {
        // If content cache hasn't been initialized properly, make the query
        // failed.
        MOZ_LOG(sContentCacheLog, LogLevel::Error,
          ("ContentCacheInParent: 0x%p HandleQueryContentEvent(), "
           "FAILED because mSelection is not valid", this));
        return true;
      }
      if (NS_WARN_IF(!GetCaretRect(aEvent.mInput.mOffset,
                                   aEvent.mReply.mRect))) {
        MOZ_LOG(sContentCacheLog, LogLevel::Error,
          ("ContentCacheInParent: 0x%p HandleQueryContentEvent(), "
           "FAILED to get caret rect", this));
        return false;
      }
      aEvent.mReply.mOffset = aEvent.mInput.mOffset;
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
        ("ContentCacheInParent: 0x%p HandleQueryContentEvent(), "
         "Succeeded, aEvent={ mReply={ mOffset=%u, mRect=%s } }",
         this, aEvent.mReply.mOffset, GetRectText(aEvent.mReply.mRect).get()));
      break;
    case eQueryEditorRect:
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
        ("ContentCacheInParent: 0x%p HandleQueryContentEvent("
         "aEvent={ mMessage=eQueryEditorRect }, aWidget=0x%p)",
         this, aWidget));
      aEvent.mReply.mRect = mEditorRect;
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
        ("ContentCacheInParent: 0x%p HandleQueryContentEvent(), "
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
                                  LayoutDeviceIntRect& aTextRect) const
{
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("ContentCacheInParent: 0x%p GetTextRect(aOffset=%u), "
     "mTextRectArray={ mStart=%u, mRects.Length()=%u }, "
     "mSelection={ mAnchor=%u, mFocus=%u }",
     this, aOffset, mTextRectArray.mStart, mTextRectArray.mRects.Length(),
     mSelection.mAnchor, mSelection.mFocus));

  if (!aOffset) {
    NS_WARN_IF(mFirstCharRect.IsEmpty());
    aTextRect = mFirstCharRect;
    return !aTextRect.IsEmpty();
  }
  if (aOffset == mSelection.mAnchor) {
    NS_WARN_IF(mSelection.mAnchorCharRect.IsEmpty());
    aTextRect = mSelection.mAnchorCharRect;
    return !aTextRect.IsEmpty();
  }
  if (aOffset == mSelection.mFocus) {
    NS_WARN_IF(mSelection.mFocusCharRect.IsEmpty());
    aTextRect = mSelection.mFocusCharRect;
    return !aTextRect.IsEmpty();
  }

  if (!mTextRectArray.InRange(aOffset)) {
    aTextRect.SetEmpty();
    return false;
  }
  aTextRect = mTextRectArray.GetRect(aOffset);
  return true;
}

bool
ContentCacheInParent::GetUnionTextRects(
                        uint32_t aOffset,
                        uint32_t aLength,
                        LayoutDeviceIntRect& aUnionTextRect) const
{
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("ContentCacheInParent: 0x%p GetUnionTextRects(aOffset=%u, "
     "aLength=%u), mTextRectArray={ mStart=%u, mRects.Length()=%u }, "
     "mSelection={ mAnchor=%u, mFocus=%u }",
     this, aOffset, aLength, mTextRectArray.mStart,
     mTextRectArray.mRects.Length(), mSelection.mAnchor, mSelection.mFocus));

  CheckedInt<uint32_t> endOffset =
    CheckedInt<uint32_t>(aOffset) + aLength;
  if (!endOffset.isValid()) {
    return false;
  }

  if (!mSelection.Collapsed() &&
      aOffset == mSelection.StartOffset() && aLength == mSelection.Length()) {
    NS_WARN_IF(mSelection.mRect.IsEmpty());
    aUnionTextRect = mSelection.mRect;
    return !aUnionTextRect.IsEmpty();
  }

  if (aLength == 1) {
    if (!aOffset) {
      NS_WARN_IF(mFirstCharRect.IsEmpty());
      aUnionTextRect = mFirstCharRect;
      return !aUnionTextRect.IsEmpty();
    }
    if (aOffset == mSelection.mAnchor) {
      NS_WARN_IF(mSelection.mAnchorCharRect.IsEmpty());
      aUnionTextRect = mSelection.mAnchorCharRect;
      return !aUnionTextRect.IsEmpty();
    }
    if (aOffset == mSelection.mFocus) {
      NS_WARN_IF(mSelection.mFocusCharRect.IsEmpty());
      aUnionTextRect = mSelection.mFocusCharRect;
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

  if (mTextRectArray.IsOverlappingWith(aOffset, aLength)) {
    aUnionTextRect =
      mTextRectArray.GetUnionRectAsFarAsPossible(aOffset, aLength);
  } else {
    aUnionTextRect.SetEmpty();
  }

  if (!aOffset) {
    aUnionTextRect = aUnionTextRect.Union(mFirstCharRect);
  }
  if (aOffset <= mSelection.mAnchor && mSelection.mAnchor < endOffset.value()) {
    aUnionTextRect = aUnionTextRect.Union(mSelection.mAnchorCharRect);
  }
  if (aOffset <= mSelection.mFocus && mSelection.mFocus < endOffset.value()) {
    aUnionTextRect = aUnionTextRect.Union(mSelection.mFocusCharRect);
  }
  return !aUnionTextRect.IsEmpty();
}

bool
ContentCacheInParent::GetCaretRect(uint32_t aOffset,
                                   LayoutDeviceIntRect& aCaretRect) const
{
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("ContentCacheInParent: 0x%p GetCaretRect(aOffset=%u), "
     "mCaret={ mOffset=%u, mRect=%s, IsValid()=%s }, mTextRectArray={ "
     "mStart=%u, mRects.Length()=%u }, mSelection={ mAnchor=%u, mFocus=%u, "
     "mWritingMode=%s, mAnchorCharRect=%s, mFocusCharRect=%s }, "
     "mFirstCharRect=%s",
     this, aOffset, mCaret.mOffset, GetRectText(mCaret.mRect).get(),
     GetBoolName(mCaret.IsValid()), mTextRectArray.mStart,
     mTextRectArray.mRects.Length(), mSelection.mAnchor, mSelection.mFocus,
     GetWritingModeName(mSelection.mWritingMode).get(),
     GetRectText(mSelection.mAnchorCharRect).get(),
     GetRectText(mSelection.mFocusCharRect).get(),
     GetRectText(mFirstCharRect).get()));

  if (mCaret.IsValid() && mCaret.mOffset == aOffset) {
    aCaretRect = mCaret.mRect;
    return true;
  }

  // Guess caret rect from the text rect if it's stored.
  if (!GetTextRect(aOffset, aCaretRect)) {
    // There might be previous character rect in the cache.  If so, we can
    // guess the caret rect with it.
    if (!aOffset || !GetTextRect(aOffset - 1, aCaretRect)) {
      aCaretRect.SetEmpty();
      return false;
    }

    if (mSelection.mWritingMode.IsVertical()) {
      aCaretRect.y = aCaretRect.YMost();
    } else {
      // XXX bidi-unaware.
      aCaretRect.x = aCaretRect.XMost();
    }
  }

  // XXX This is not bidi aware because we don't cache each character's
  //     direction.  However, this is usually used by IME, so, assuming the
  //     character is in LRT context must not cause any problem.
  if (mSelection.mWritingMode.IsVertical()) {
    aCaretRect.height = mCaret.IsValid() ? mCaret.mRect.height : 1;
  } else {
    aCaretRect.width = mCaret.IsValid() ? mCaret.mRect.width : 1;
  }
  return true;
}

bool
ContentCacheInParent::OnCompositionEvent(const WidgetCompositionEvent& aEvent)
{
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("ContentCacheInParent: 0x%p OnCompositionEvent(aEvent={ "
     "mMessage=%s, mData=\"%s\" (Length()=%u), mRanges->Length()=%u }), "
     "mPendingEventsNeedingAck=%u, mIsComposing=%s, "
     "mCommitStringByRequest=0x%p",
     this, ToChar(aEvent.mMessage),
     NS_ConvertUTF16toUTF8(aEvent.mData).get(), aEvent.mData.Length(),
     aEvent.mRanges ? aEvent.mRanges->Length() : 0, mPendingEventsNeedingAck,
     GetBoolName(mIsComposing), mCommitStringByRequest));

  // We must be able to simulate the selection because
  // we might not receive selection updates in time
  if (!mIsComposing) {
    mCompositionStart = mSelection.StartOffset();
  }

  mIsComposing = !aEvent.CausesDOMCompositionEndEvent();

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
    ("ContentCacheInParent: 0x%p OnSelectionEvent(aEvent={ "
     "mMessage=%s, mOffset=%u, mLength=%u, mReversed=%s, "
     "mExpandToClusterBoundary=%s, mUseNativeLineBreak=%s }), "
     "mPendingEventsNeedingAck=%u, mIsComposing=%s",
     this, ToChar(aSelectionEvent.mMessage),
     aSelectionEvent.mOffset, aSelectionEvent.mLength,
     GetBoolName(aSelectionEvent.mReversed),
     GetBoolName(aSelectionEvent.mExpandToClusterBoundary),
     GetBoolName(aSelectionEvent.mUseNativeLineBreak), mPendingEventsNeedingAck,
     GetBoolName(mIsComposing)));

  mPendingEventsNeedingAck++;
}

void
ContentCacheInParent::OnEventNeedingAckHandled(nsIWidget* aWidget,
                                                EventMessage aMessage)
{
  // This is called when the child process receives WidgetCompositionEvent or
  // WidgetSelectionEvent.

  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("ContentCacheInParent: 0x%p OnEventNeedingAckHandled(aWidget=0x%p, "
     "aMessage=%s), mPendingEventsNeedingAck=%u",
     this, aWidget, ToChar(aMessage), mPendingEventsNeedingAck));

  MOZ_RELEASE_ASSERT(mPendingEventsNeedingAck > 0);
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
    ("ContentCacheInParent: 0x%p RequestToCommitComposition(aWidget=%p, "
     "aCancel=%s), mIsComposing=%s, mCommitStringByRequest=%p",
     this, aWidget, GetBoolName(aCancel), GetBoolName(mIsComposing),
     mCommitStringByRequest));

  MOZ_ASSERT(!mCommitStringByRequest);

  RefPtr<TextComposition> composition =
    IMEStateManager::GetTextCompositionFor(aWidget);
  if (NS_WARN_IF(!composition)) {
    MOZ_LOG(sContentCacheLog, LogLevel::Warning,
      ("  ContentCacheInParent: 0x%p RequestToCommitComposition(), "
       "does nothing due to no composition", this));
    return false;
  }

  mCommitStringByRequest = &aCommittedString;

  aWidget->NotifyIME(IMENotification(aCancel ? REQUEST_TO_CANCEL_COMPOSITION :
                                               REQUEST_TO_COMMIT_COMPOSITION));

  mCommitStringByRequest = nullptr;

  MOZ_LOG(sContentCacheLog, LogLevel::Info,
    ("  ContentCacheInParent: 0x%p RequestToCommitComposition(), "
     "mIsComposing=%s, the composition %s committed synchronously",
     this, GetBoolName(mIsComposing),
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
    return false;
  }

  // When the composition is committed synchronously, the commit string will be
  // returned to the remote process. Then, PuppetWidget will dispatch
  // eCompositionCommit event with the returned commit string (i.e., the value
  // is aCommittedString of this method).  Finally, TextComposition instance in
  // the remote process will be destroyed by
  // IMEStateManager::DispatchCompositionEvent() at receiving the
  // eCompositionCommit event (Note that TextComposition instance in this
  // process was already destroyed).
  return true;
}

void
ContentCacheInParent::MaybeNotifyIME(nsIWidget* aWidget,
                                     const IMENotification& aNotification)
{
  if (!mPendingEventsNeedingAck) {
    IMEStateManager::NotifyIME(aNotification, aWidget, true);
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
    case NOTIFY_IME_OF_COMPOSITION_UPDATE:
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

  // New notifications which are notified during flushing pending notifications
  // should be merged again.
  mPendingEventsNeedingAck++;

  nsCOMPtr<nsIWidget> kungFuDeathGrip(aWidget);

  // First, text change notification should be sent because selection change
  // notification notifies IME of current selection range in the latest content.
  // So, IME may need the latest content before that.
  if (mPendingTextChange.HasNotification()) {
    IMENotification notification(mPendingTextChange);
    if (!aWidget->Destroyed()) {
      mPendingTextChange.Clear();
      IMEStateManager::NotifyIME(notification, aWidget, true);
    }
  }

  if (mPendingSelectionChange.HasNotification()) {
    IMENotification notification(mPendingSelectionChange);
    if (!aWidget->Destroyed()) {
      mPendingSelectionChange.Clear();
      IMEStateManager::NotifyIME(notification, aWidget, true);
    }
  }

  // Layout change notification should be notified after selection change
  // notification because IME may want to query position of new caret position.
  if (mPendingLayoutChange.HasNotification()) {
    IMENotification notification(mPendingLayoutChange);
    if (!aWidget->Destroyed()) {
      mPendingLayoutChange.Clear();
      IMEStateManager::NotifyIME(notification, aWidget, true);
    }
  }

  // Finally, send composition update notification because it notifies IME of
  // finishing handling whole sending events.
  if (mPendingCompositionUpdate.HasNotification()) {
    IMENotification notification(mPendingCompositionUpdate);
    if (!aWidget->Destroyed()) {
      mPendingCompositionUpdate.Clear();
      IMEStateManager::NotifyIME(notification, aWidget, true);
    }
  }

  if (!--mPendingEventsNeedingAck && !aWidget->Destroyed() &&
      (mPendingTextChange.HasNotification() ||
       mPendingSelectionChange.HasNotification() ||
       mPendingLayoutChange.HasNotification() ||
       mPendingCompositionUpdate.HasNotification())) {
    FlushPendingNotifications(aWidget);
  }
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

LayoutDeviceIntRect
ContentCache::TextRectArray::GetUnionRectAsFarAsPossible(
                               uint32_t aOffset,
                               uint32_t aLength) const
{
  LayoutDeviceIntRect rect;
  if (!IsOverlappingWith(aOffset, aLength)) {
    return rect;
  }
  uint32_t startOffset = std::max(aOffset, mStart);
  uint32_t endOffset = std::min(aOffset + aLength, EndOffset());
  for (uint32_t i = 0; i < endOffset - startOffset; i++) {
    rect = rect.Union(mRects[startOffset - mStart + i]);
  }
  return rect;
}

} // namespace mozilla
