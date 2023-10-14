/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentCache.h"

#include <utility>

#include "IMEData.h"
#include "TextEvents.h"

#include "mozilla/Assertions.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Logging.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TextComposition.h"
#include "mozilla/dom/BrowserParent.h"
#include "nsExceptionHandler.h"
#include "nsIWidget.h"
#include "nsPrintfCString.h"

namespace mozilla {

using namespace dom;
using namespace widget;

static const char* GetBoolName(bool aBool) { return aBool ? "true" : "false"; }

static const char* GetNotificationName(const IMENotification* aNotification) {
  if (!aNotification) {
    return "Not notification";
  }
  return ToChar(aNotification->mMessage);
}

/*****************************************************************************
 * mozilla::ContentCache
 *****************************************************************************/

LazyLogModule sContentCacheLog("ContentCacheWidgets");

bool ContentCache::IsValid() const {
  if (mText.isNothing()) {
    // mSelection and mCaret depend on mText.
    if (NS_WARN_IF(mSelection.isSome()) || NS_WARN_IF(mCaret.isSome())) {
      return false;
    }
  } else {
    // mSelection depends on mText.
    if (mSelection.isSome() && NS_WARN_IF(!mSelection->IsValidIn(*mText))) {
      return false;
    }

    // mCaret depends on mSelection.
    if (mCaret.isSome() &&
        (NS_WARN_IF(mSelection.isNothing()) ||
         NS_WARN_IF(!mSelection->mHasRange) ||
         NS_WARN_IF(mSelection->StartOffset() != mCaret->Offset()))) {
      return false;
    }
  }

  // mTextRectArray stores character rects around composition string.
  // Note that even if we fail to collect the rects, we may keep storing
  // mCompositionStart.
  if (mTextRectArray.isSome()) {
    if (NS_WARN_IF(mCompositionStart.isNothing())) {
      return false;
    }
  }

  return true;
}

void ContentCache::AssertIfInvalid() const {
#if MOZ_DIAGNOSTIC_ASSERT_ENABLED
  if (IsValid()) {
    return;
  }

  // This text will appear in the crash reports without any permissions.
  // Do not use `ToString` here to avoid to expose unexpected data with
  // changing the type or `operator<<()`.
  nsPrintfCString info(
      "ContentCache={ mText=%s, mSelection=%s, mCaret=%s, mTextRectArray=%s, "
      "mCompositionStart=%s }\n",
      // Don't expose mText.ref() value for protecting the user's privacy.
      mText.isNothing()
          ? "Nothing"
          : nsPrintfCString("{ Length()=%zu }", mText->Length()).get(),
      mSelection.isNothing()
          ? "Nothing"
          : nsPrintfCString("{ mAnchor=%u, mFocus=%u }", mSelection->mAnchor,
                            mSelection->mFocus)
                .get(),
      mCaret.isNothing()
          ? "Nothing"
          : nsPrintfCString("{ mOffset=%u }", mCaret->mOffset).get(),
      mTextRectArray.isNothing()
          ? "Nothing"
          : nsPrintfCString("{ Length()=%u }", mTextRectArray->Length()).get(),
      mCompositionStart.isNothing()
          ? "Nothing"
          : nsPrintfCString("%u", mCompositionStart.value()).get());
  CrashReporter::AppendAppNotesToCrashReport(info);
  MOZ_DIAGNOSTIC_ASSERT(false, "Invalid ContentCache data");
#endif  // #if MOZ_DIAGNOSTIC_ASSERT_ENABLED
}

/*****************************************************************************
 * mozilla::ContentCacheInChild
 *****************************************************************************/

void ContentCacheInChild::Clear() {
  MOZ_LOG(sContentCacheLog, LogLevel::Info, ("0x%p Clear()", this));

  mCompositionStart.reset();
  mLastCommit.reset();
  mText.reset();
  mSelection.reset();
  mFirstCharRect.SetEmpty();
  mCaret.reset();
  mTextRectArray.reset();
  mLastCommitStringTextRectArray.reset();
  mEditorRect.SetEmpty();
}

void ContentCacheInChild::OnCompositionEvent(
    const WidgetCompositionEvent& aCompositionEvent) {
  if (aCompositionEvent.CausesDOMCompositionEndEvent()) {
    RefPtr<TextComposition> composition =
        IMEStateManager::GetTextCompositionFor(aCompositionEvent.mWidget);
    if (composition) {
      nsAutoString lastCommitString;
      if (aCompositionEvent.mMessage == eCompositionCommitAsIs) {
        lastCommitString = composition->CommitStringIfCommittedAsIs();
      } else {
        lastCommitString = aCompositionEvent.mData;
      }
      // We don't need to store canceling information because this is required
      // by undoing of last commit (Kakutei-Undo of Japanese IME).
      if (!lastCommitString.IsEmpty()) {
        mLastCommit = Some(OffsetAndData<uint32_t>(
            composition->NativeOffsetOfStartComposition(), lastCommitString));
        MOZ_LOG(
            sContentCacheLog, LogLevel::Debug,
            ("0x%p OnCompositionEvent(), stored last composition string data "
             "(aCompositionEvent={ mMessage=%s, mData=\"%s\"}, mLastCommit=%s)",
             this, ToChar(aCompositionEvent.mMessage),
             PrintStringDetail(
                 aCompositionEvent.mData,
                 PrintStringDetail::kMaxLengthForCompositionString)
                 .get(),
             ToString(mLastCommit).c_str()));
        return;
      }
    }
  }
  if (mLastCommit.isSome()) {
    MOZ_LOG(
        sContentCacheLog, LogLevel::Debug,
        ("0x%p OnCompositionEvent(), resetting the last composition string "
         "data (aCompositionEvent={ mMessage=%s, mData=\"%s\"}, "
         "mLastCommit=%s)",
         this, ToChar(aCompositionEvent.mMessage),
         PrintStringDetail(aCompositionEvent.mData,
                           PrintStringDetail::kMaxLengthForCompositionString)
             .get(),
         ToString(mLastCommit).c_str()));
    mLastCommit.reset();
  }
}

bool ContentCacheInChild::CacheAll(nsIWidget* aWidget,
                                   const IMENotification* aNotification) {
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
          ("0x%p CacheAll(aWidget=0x%p, aNotification=%s)", this, aWidget,
           GetNotificationName(aNotification)));

  const bool textCached = CacheText(aWidget, aNotification);
  const bool editorRectCached = CacheEditorRect(aWidget, aNotification);
  AssertIfInvalid();
  return (textCached || editorRectCached) && IsValid();
}

bool ContentCacheInChild::CacheSelection(nsIWidget* aWidget,
                                         const IMENotification* aNotification) {
  MOZ_LOG(
      sContentCacheLog, LogLevel::Info,
      ("0x%p CacheSelection(aWidget=0x%p, aNotification=%s), mText=%s", this,
       aWidget, GetNotificationName(aNotification),
       PrintStringDetail(mText, PrintStringDetail::kMaxLengthForEditor).get()));

  mSelection.reset();
  mCaret.reset();

  if (mText.isNothing()) {
    return false;
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetQueryContentEvent querySelectedTextEvent(true, eQuerySelectedText,
                                                 aWidget);
  aWidget->DispatchEvent(&querySelectedTextEvent, status);
  if (NS_WARN_IF(querySelectedTextEvent.Failed())) {
    MOZ_LOG(
        sContentCacheLog, LogLevel::Error,
        ("0x%p CacheSelection(), FAILED, couldn't retrieve the selected text",
         this));
    // XXX Allowing selection-independent character rects makes things
    //     complicated in the parent...
  }
  // ContentCache should store only editable content.  Therefore, if current
  // selection root is not editable, we don't need to store the selection, i.e.,
  // let's treat it as there is no selection.  However, if we already have
  // previously editable text, let's store the selection even if it becomes
  // uneditable because not doing so would create odd situation.  E.g., IME may
  // fail only querying selection after succeeded querying text.
  else if (NS_WARN_IF(!querySelectedTextEvent.mReply->mIsEditableContent)) {
    MOZ_LOG(sContentCacheLog, LogLevel::Error,
            ("0x%p CacheSelection(), FAILED, editable content had already been "
             "blurred",
             this));
    AssertIfInvalid();
    return false;
  } else {
    mSelection.emplace(querySelectedTextEvent);
  }

  return (CacheCaretAndTextRects(aWidget, aNotification) ||
          querySelectedTextEvent.Succeeded()) &&
         IsValid();
}

bool ContentCacheInChild::CacheCaret(nsIWidget* aWidget,
                                     const IMENotification* aNotification) {
  mCaret.reset();

  if (mSelection.isNothing()) {
    return false;
  }

  MOZ_LOG(sContentCacheLog, LogLevel::Info,
          ("0x%p CacheCaret(aWidget=0x%p, aNotification=%s)", this, aWidget,
           GetNotificationName(aNotification)));

  if (mSelection->mHasRange) {
    // XXX Should be mSelection.mFocus?
    const uint32_t offset = mSelection->StartOffset();

    nsEventStatus status = nsEventStatus_eIgnore;
    WidgetQueryContentEvent queryCaretRectEvent(true, eQueryCaretRect, aWidget);
    queryCaretRectEvent.InitForQueryCaretRect(offset);
    aWidget->DispatchEvent(&queryCaretRectEvent, status);
    if (NS_WARN_IF(queryCaretRectEvent.Failed())) {
      MOZ_LOG(sContentCacheLog, LogLevel::Error,
              ("0x%p   CacheCaret(), FAILED, couldn't retrieve the caret rect "
               "at offset=%u",
               this, offset));
      return false;
    }
    mCaret.emplace(offset, queryCaretRectEvent.mReply->mRect);
  }
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
          ("0x%p   CacheCaret(), Succeeded, mSelection=%s, mCaret=%s", this,
           ToString(mSelection).c_str(), ToString(mCaret).c_str()));
  AssertIfInvalid();
  return IsValid();
}

bool ContentCacheInChild::CacheEditorRect(
    nsIWidget* aWidget, const IMENotification* aNotification) {
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
          ("0x%p CacheEditorRect(aWidget=0x%p, aNotification=%s)", this,
           aWidget, GetNotificationName(aNotification)));

  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetQueryContentEvent queryEditorRectEvent(true, eQueryEditorRect, aWidget);
  aWidget->DispatchEvent(&queryEditorRectEvent, status);
  if (NS_WARN_IF(queryEditorRectEvent.Failed())) {
    MOZ_LOG(
        sContentCacheLog, LogLevel::Error,
        ("0x%p   CacheEditorRect(), FAILED, couldn't retrieve the editor rect",
         this));
    return false;
  }
  // ContentCache should store only editable content.  Therefore, if current
  // selection root is not editable, we don't need to store the editor rect,
  // i.e., let's treat it as there is no focused editor.
  if (NS_WARN_IF(!queryEditorRectEvent.mReply->mIsEditableContent)) {
    MOZ_LOG(sContentCacheLog, LogLevel::Error,
            ("0x%p   CacheText(), FAILED, editable content had already been "
             "blurred",
             this));
    return false;
  }
  mEditorRect = queryEditorRectEvent.mReply->mRect;
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
          ("0x%p   CacheEditorRect(), Succeeded, mEditorRect=%s", this,
           ToString(mEditorRect).c_str()));
  return true;
}

bool ContentCacheInChild::CacheCaretAndTextRects(
    nsIWidget* aWidget, const IMENotification* aNotification) {
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
          ("0x%p CacheCaretAndTextRects(aWidget=0x%p, aNotification=%s)", this,
           aWidget, GetNotificationName(aNotification)));

  const bool caretCached = CacheCaret(aWidget, aNotification);
  const bool textRectsCached = CacheTextRects(aWidget, aNotification);
  AssertIfInvalid();
  return (caretCached || textRectsCached) && IsValid();
}

bool ContentCacheInChild::CacheText(nsIWidget* aWidget,
                                    const IMENotification* aNotification) {
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
          ("0x%p CacheText(aWidget=0x%p, aNotification=%s)", this, aWidget,
           GetNotificationName(aNotification)));

  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetQueryContentEvent queryTextContentEvent(true, eQueryTextContent,
                                                aWidget);
  queryTextContentEvent.InitForQueryTextContent(0, UINT32_MAX);
  aWidget->DispatchEvent(&queryTextContentEvent, status);
  if (NS_WARN_IF(queryTextContentEvent.Failed())) {
    MOZ_LOG(sContentCacheLog, LogLevel::Error,
            ("0x%p   CacheText(), FAILED, couldn't retrieve whole text", this));
    mText.reset();
  }
  // ContentCache should store only editable content.  Therefore, if current
  // selection root is not editable, we don't need to store the text, i.e.,
  // let's treat it as there is no editable text.
  else if (NS_WARN_IF(!queryTextContentEvent.mReply->mIsEditableContent)) {
    MOZ_LOG(sContentCacheLog, LogLevel::Error,
            ("0x%p   CacheText(), FAILED, editable content had already been "
             "blurred",
             this));
    mText.reset();
  } else {
    mText = Some(nsString(queryTextContentEvent.mReply->DataRef()));
    MOZ_LOG(sContentCacheLog, LogLevel::Info,
            ("0x%p   CacheText(), Succeeded, mText=%s", this,
             PrintStringDetail(mText, PrintStringDetail::kMaxLengthForEditor)
                 .get()));
  }

  // Forget last commit range if string in the range is different from the
  // last commit string.
  if (mLastCommit.isSome() &&
      (mText.isNothing() ||
       nsDependentSubstring(mText.ref(), mLastCommit->StartOffset(),
                            mLastCommit->Length()) != mLastCommit->DataRef())) {
    MOZ_LOG(sContentCacheLog, LogLevel::Debug,
            ("0x%p   CacheText(), resetting the last composition string data "
             "(mLastCommit=%s, current string=\"%s\")",
             this, ToString(mLastCommit).c_str(),
             PrintStringDetail(
                 nsDependentSubstring(mText.ref(), mLastCommit->StartOffset(),
                                      mLastCommit->Length()),
                 PrintStringDetail::kMaxLengthForCompositionString)
                 .get()));
    mLastCommit.reset();
  }

  // If we fail to get editable text content, it must mean that there is no
  // focused element anymore or focused element is not editable.  In this case,
  // we should not get selection of non-editable content
  if (MOZ_UNLIKELY(mText.isNothing())) {
    mSelection.reset();
    mCaret.reset();
    mTextRectArray.reset();
    AssertIfInvalid();
    return false;
  }

  return CacheSelection(aWidget, aNotification);
}

bool ContentCacheInChild::QueryCharRect(nsIWidget* aWidget, uint32_t aOffset,
                                        LayoutDeviceIntRect& aCharRect) const {
  aCharRect.SetEmpty();

  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetQueryContentEvent queryTextRectEvent(true, eQueryTextRect, aWidget);
  queryTextRectEvent.InitForQueryTextRect(aOffset, 1);
  aWidget->DispatchEvent(&queryTextRectEvent, status);
  if (NS_WARN_IF(queryTextRectEvent.Failed())) {
    return false;
  }
  aCharRect = queryTextRectEvent.mReply->mRect;

  // Guarantee the rect is not empty.
  if (NS_WARN_IF(!aCharRect.Height())) {
    aCharRect.SetHeight(1);
  }
  if (NS_WARN_IF(!aCharRect.Width())) {
    aCharRect.SetWidth(1);
  }
  return true;
}

bool ContentCacheInChild::QueryCharRectArray(nsIWidget* aWidget,
                                             uint32_t aOffset, uint32_t aLength,
                                             RectArray& aCharRectArray) const {
  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetQueryContentEvent queryTextRectsEvent(true, eQueryTextRectArray,
                                              aWidget);
  queryTextRectsEvent.InitForQueryTextRectArray(aOffset, aLength);
  aWidget->DispatchEvent(&queryTextRectsEvent, status);
  if (NS_WARN_IF(queryTextRectsEvent.Failed())) {
    aCharRectArray.Clear();
    return false;
  }
  aCharRectArray = std::move(queryTextRectsEvent.mReply->mRectArray);
  return true;
}

bool ContentCacheInChild::CacheTextRects(nsIWidget* aWidget,
                                         const IMENotification* aNotification) {
  MOZ_LOG(
      sContentCacheLog, LogLevel::Info,
      ("0x%p CacheTextRects(aWidget=0x%p, aNotification=%s), mCaret=%s", this,
       aWidget, GetNotificationName(aNotification), ToString(mCaret).c_str()));

  if (mSelection.isSome()) {
    mSelection->ClearRects();
  }

  // Retrieve text rects in composition string if there is.
  RefPtr<TextComposition> textComposition =
      IMEStateManager::GetTextCompositionFor(aWidget);
  if (textComposition) {
    // mCompositionStart may be updated by some composition event handlers.
    // So, let's update it with the latest information.
    mCompositionStart = Some(textComposition->NativeOffsetOfStartComposition());
    // Note that TextComposition::String() may not be modified here because
    // it's modified after all edit action listeners are performed but this
    // is called while some of them are performed.
    // FYI: For supporting IME which commits composition and restart new
    //      composition immediately, we should cache next character of current
    //      composition too.
    uint32_t length = textComposition->LastData().Length() + 1;
    mTextRectArray = Some(TextRectArray(mCompositionStart.value()));
    if (NS_WARN_IF(!QueryCharRectArray(aWidget, mTextRectArray->mStart, length,
                                       mTextRectArray->mRects))) {
      MOZ_LOG(sContentCacheLog, LogLevel::Error,
              ("0x%p   CacheTextRects(), FAILED, "
               "couldn't retrieve text rect array of the composition string",
               this));
      mTextRectArray.reset();
    }
  } else {
    mCompositionStart.reset();
    mTextRectArray.reset();
  }

  if (mSelection.isSome()) {
    // Set mSelection->mAnchorCharRects
    // If we've already have the rect in mTextRectArray, save the query cost.
    if (mSelection->mHasRange && mTextRectArray.isSome() &&
        mTextRectArray->IsOffsetInRange(mSelection->mAnchor) &&
        (!mSelection->mAnchor ||
         mTextRectArray->IsOffsetInRange(mSelection->mAnchor - 1))) {
      mSelection->mAnchorCharRects[eNextCharRect] =
          mTextRectArray->GetRect(mSelection->mAnchor);
      if (mSelection->mAnchor) {
        mSelection->mAnchorCharRects[ePrevCharRect] =
            mTextRectArray->GetRect(mSelection->mAnchor - 1);
      }
    }
    // Otherwise, get it from content even if there is no selection ranges.
    else {
      RectArray rects;
      const uint32_t startOffset = mSelection->mHasRange && mSelection->mAnchor
                                       ? mSelection->mAnchor - 1u
                                       : 0u;
      const uint32_t length =
          mSelection->mHasRange && mSelection->mAnchor ? 2u : 1u;
      if (NS_WARN_IF(
              !QueryCharRectArray(aWidget, startOffset, length, rects))) {
        MOZ_LOG(
            sContentCacheLog, LogLevel::Error,
            ("0x%p   CacheTextRects(), FAILED, couldn't retrieve text rect "
             "array around the selection anchor (%s)",
             this,
             mSelection ? ToString(mSelection->mAnchor).c_str() : "Nothing"));
        MOZ_ASSERT_IF(mSelection.isSome(),
                      mSelection->mAnchorCharRects[ePrevCharRect].IsEmpty());
        MOZ_ASSERT_IF(mSelection.isSome(),
                      mSelection->mAnchorCharRects[eNextCharRect].IsEmpty());
      } else if (rects.Length()) {
        if (rects.Length() > 1) {
          mSelection->mAnchorCharRects[ePrevCharRect] = rects[0];
          mSelection->mAnchorCharRects[eNextCharRect] = rects[1];
        } else {
          mSelection->mAnchorCharRects[eNextCharRect] = rects[0];
          MOZ_ASSERT(mSelection->mAnchorCharRects[ePrevCharRect].IsEmpty());
        }
      }
    }

    // Set mSelection->mFocusCharRects
    // If selection is collapsed (including no selection case), the focus char
    // rects are same as the anchor char rects so that we can just copy them.
    if (mSelection->IsCollapsed()) {
      mSelection->mFocusCharRects[0] = mSelection->mAnchorCharRects[0];
      mSelection->mFocusCharRects[1] = mSelection->mAnchorCharRects[1];
    }
    // If the selection range is in mTextRectArray, save the query cost.
    else if (mTextRectArray.isSome() &&
             mTextRectArray->IsOffsetInRange(mSelection->mFocus) &&
             (!mSelection->mFocus ||
              mTextRectArray->IsOffsetInRange(mSelection->mFocus - 1))) {
      MOZ_ASSERT(mSelection->mHasRange);
      mSelection->mFocusCharRects[eNextCharRect] =
          mTextRectArray->GetRect(mSelection->mFocus);
      if (mSelection->mFocus) {
        mSelection->mFocusCharRects[ePrevCharRect] =
            mTextRectArray->GetRect(mSelection->mFocus - 1);
      }
    }
    // Otherwise, including no selection range cases, need to query the rects.
    else {
      MOZ_ASSERT(mSelection->mHasRange);
      RectArray rects;
      const uint32_t startOffset =
          mSelection->mFocus ? mSelection->mFocus - 1u : 0u;
      const uint32_t length = mSelection->mFocus ? 2u : 1u;
      if (NS_WARN_IF(
              !QueryCharRectArray(aWidget, startOffset, length, rects))) {
        MOZ_LOG(
            sContentCacheLog, LogLevel::Error,
            ("0x%p   CacheTextRects(), FAILED, couldn't retrieve text rect "
             "array around the selection focus (%s)",
             this,
             mSelection ? ToString(mSelection->mFocus).c_str() : "Nothing"));
        MOZ_ASSERT_IF(mSelection.isSome(),
                      mSelection->mFocusCharRects[ePrevCharRect].IsEmpty());
        MOZ_ASSERT_IF(mSelection.isSome(),
                      mSelection->mFocusCharRects[eNextCharRect].IsEmpty());
      } else if (NS_WARN_IF(mSelection.isNothing())) {
        MOZ_LOG(sContentCacheLog, LogLevel::Error,
                ("0x%p   CacheTextRects(), FAILED, mSelection was reset during "
                 "the call of QueryCharRectArray",
                 this));
      } else {
        if (rects.Length() > 1) {
          mSelection->mFocusCharRects[ePrevCharRect] = rects[0];
          mSelection->mFocusCharRects[eNextCharRect] = rects[1];
        } else if (rects.Length()) {
          mSelection->mFocusCharRects[eNextCharRect] = rects[0];
          MOZ_ASSERT(mSelection->mFocusCharRects[ePrevCharRect].IsEmpty());
        }
      }
    }
  }

  // If there is a non-collapsed selection range, let's query the whole selected
  // text rect.  Note that the result cannot be computed from first character
  // rect and last character rect of the selection because they both may be in
  // middle of different line.
  if (mSelection.isSome() && mSelection->mHasRange &&
      !mSelection->IsCollapsed()) {
    nsEventStatus status = nsEventStatus_eIgnore;
    WidgetQueryContentEvent queryTextRectEvent(true, eQueryTextRect, aWidget);
    queryTextRectEvent.InitForQueryTextRect(mSelection->StartOffset(),
                                            mSelection->Length());
    aWidget->DispatchEvent(&queryTextRectEvent, status);
    if (NS_WARN_IF(queryTextRectEvent.Failed())) {
      MOZ_LOG(sContentCacheLog, LogLevel::Error,
              ("0x%p   CacheTextRects(), FAILED, "
               "couldn't retrieve text rect of whole selected text",
               this));
    } else {
      mSelection->mRect = queryTextRectEvent.mReply->mRect;
    }
  }

  // Even if there is no selection range, we should have the first character
  // rect for the last resort of suggesting position of IME UI.
  if (mSelection.isSome() && mSelection->mHasRange && !mSelection->mFocus) {
    mFirstCharRect = mSelection->mFocusCharRects[eNextCharRect];
  } else if (mSelection.isSome() && mSelection->mHasRange &&
             mSelection->mFocus == 1) {
    mFirstCharRect = mSelection->mFocusCharRects[ePrevCharRect];
  } else if (mSelection.isSome() && mSelection->mHasRange &&
             !mSelection->mAnchor) {
    mFirstCharRect = mSelection->mAnchorCharRects[eNextCharRect];
  } else if (mSelection.isSome() && mSelection->mHasRange &&
             mSelection->mAnchor == 1) {
    mFirstCharRect = mSelection->mFocusCharRects[ePrevCharRect];
  } else if (mTextRectArray.isSome() && mTextRectArray->IsOffsetInRange(0u)) {
    mFirstCharRect = mTextRectArray->GetRect(0u);
  } else {
    LayoutDeviceIntRect charRect;
    if (MOZ_UNLIKELY(NS_WARN_IF(!QueryCharRect(aWidget, 0, charRect)))) {
      MOZ_LOG(sContentCacheLog, LogLevel::Error,
              ("0x%p   CacheTextRects(), FAILED, "
               "couldn't retrieve first char rect",
               this));
      mFirstCharRect.SetEmpty();
    } else {
      mFirstCharRect = charRect;
    }
  }

  // Finally, let's cache the last commit string's character rects until
  // selection change or something other editing because user may reconvert
  // or undo the last commit.  Then, IME requires the character rects for
  // positioning their UI.
  if (mLastCommit.isSome()) {
    mLastCommitStringTextRectArray =
        Some(TextRectArray(mLastCommit->StartOffset()));
    if (mLastCommit->Length() == 1 && mSelection.isSome() &&
        mSelection->mHasRange &&
        mSelection->mAnchor - 1 == mLastCommit->StartOffset() &&
        !mSelection->mAnchorCharRects[ePrevCharRect].IsEmpty()) {
      mLastCommitStringTextRectArray->mRects.AppendElement(
          mSelection->mAnchorCharRects[ePrevCharRect]);
    } else if (NS_WARN_IF(!QueryCharRectArray(
                   aWidget, mLastCommit->StartOffset(), mLastCommit->Length(),
                   mLastCommitStringTextRectArray->mRects))) {
      MOZ_LOG(sContentCacheLog, LogLevel::Error,
              ("0x%p   CacheTextRects(), FAILED, "
               "couldn't retrieve text rect array of the last commit string",
               this));
      mLastCommitStringTextRectArray.reset();
      mLastCommit.reset();
    }
    MOZ_ASSERT((mLastCommitStringTextRectArray.isSome()
                    ? mLastCommitStringTextRectArray->mRects.Length()
                    : 0) == (mLastCommit.isSome() ? mLastCommit->Length() : 0));
  } else {
    mLastCommitStringTextRectArray.reset();
  }

  MOZ_LOG(
      sContentCacheLog, LogLevel::Info,
      ("0x%p   CacheTextRects(), Succeeded, "
       "mText=%s, mTextRectArray=%s, mSelection=%s, "
       "mFirstCharRect=%s, mLastCommitStringTextRectArray=%s",
       this,
       PrintStringDetail(mText, PrintStringDetail::kMaxLengthForEditor).get(),
       ToString(mTextRectArray).c_str(), ToString(mSelection).c_str(),
       ToString(mFirstCharRect).c_str(),
       ToString(mLastCommitStringTextRectArray).c_str()));
  AssertIfInvalid();
  return IsValid();
}

bool ContentCacheInChild::SetSelection(
    nsIWidget* aWidget,
    const IMENotification::SelectionChangeDataBase& aSelectionChangeData) {
  MOZ_LOG(
      sContentCacheLog, LogLevel::Info,
      ("0x%p SetSelection(aSelectionChangeData=%s), mText=%s", this,
       ToString(aSelectionChangeData).c_str(),
       PrintStringDetail(mText, PrintStringDetail::kMaxLengthForEditor).get()));

  if (MOZ_UNLIKELY(mText.isNothing())) {
    return false;
  }

  mSelection = Some(Selection(aSelectionChangeData));

  if (mLastCommit.isSome()) {
    // Forget last commit string range if selection is not collapsed
    // at end of the last commit string.
    if (!mSelection->mHasRange || !mSelection->IsCollapsed() ||
        mSelection->mAnchor != mLastCommit->EndOffset()) {
      MOZ_LOG(
          sContentCacheLog, LogLevel::Debug,
          ("0x%p   SetSelection(), forgetting last commit composition data "
           "(mSelection=%s, mLastCommit=%s)",
           this, ToString(mSelection).c_str(), ToString(mLastCommit).c_str()));
      mLastCommit.reset();
    }
  }

  CacheCaret(aWidget);
  CacheTextRects(aWidget);

  return mSelection.isSome() && IsValid();
}

/*****************************************************************************
 * mozilla::ContentCacheInParent
 *****************************************************************************/

ContentCacheInParent::ContentCacheInParent(BrowserParent& aBrowserParent)
    : mBrowserParent(aBrowserParent),
      mCommitStringByRequest(nullptr),
      mPendingCommitLength(0),
      mIsChildIgnoringCompositionEvents(false) {}

void ContentCacheInParent::AssignContent(const ContentCache& aOther,
                                         nsIWidget* aWidget,
                                         const IMENotification* aNotification) {
  MOZ_DIAGNOSTIC_ASSERT(aOther.IsValid());

  mText = aOther.mText;
  mSelection = aOther.mSelection;
  mFirstCharRect = aOther.mFirstCharRect;
  mCaret = aOther.mCaret;
  mTextRectArray = aOther.mTextRectArray;
  mLastCommitStringTextRectArray = aOther.mLastCommitStringTextRectArray;
  mEditorRect = aOther.mEditorRect;

  // Only when there is one composition, the TextComposition instance in this
  // process is managing the composition in the remote process.  Therefore,
  // we shouldn't update composition start offset of TextComposition with
  // old composition which is still being handled by the child process.
  if (WidgetHasComposition() && mHandlingCompositions.Length() == 1 &&
      mCompositionStart.isSome()) {
    IMEStateManager::MaybeStartOffsetUpdatedInChild(aWidget,
                                                    mCompositionStart.value());
  }

  // When this instance allows to query content relative to composition string,
  // we should modify mCompositionStart with the latest information in the
  // remote process because now we have the information around the composition
  // string.
  mCompositionStartInChild = aOther.mCompositionStart;
  if (WidgetHasComposition() || HasPendingCommit()) {
    if (mCompositionStartInChild.isSome()) {
      if (mCompositionStart.valueOr(UINT32_MAX) !=
          mCompositionStartInChild.value()) {
        mCompositionStart = mCompositionStartInChild;
        mPendingCommitLength = 0;
      }
    } else if (mCompositionStart.isSome() && mSelection.isSome() &&
               mSelection->mHasRange &&
               mCompositionStart.value() != mSelection->StartOffset()) {
      mCompositionStart = Some(mSelection->StartOffset());
      mPendingCommitLength = 0;
    }
  }

  MOZ_LOG(
      sContentCacheLog, LogLevel::Info,
      ("0x%p   AssignContent(aNotification=%s), "
       "Succeeded, mText=%s, mSelection=%s, mFirstCharRect=%s, "
       "mCaret=%s, mTextRectArray=%s, WidgetHasComposition()=%s, "
       "mHandlingCompositions.Length()=%zu, mCompositionStart=%s, "
       "mPendingCommitLength=%u, mEditorRect=%s, "
       "mLastCommitStringTextRectArray=%s",
       this, GetNotificationName(aNotification),
       PrintStringDetail(mText, PrintStringDetail::kMaxLengthForEditor).get(),
       ToString(mSelection).c_str(), ToString(mFirstCharRect).c_str(),
       ToString(mCaret).c_str(), ToString(mTextRectArray).c_str(),
       GetBoolName(WidgetHasComposition()), mHandlingCompositions.Length(),
       ToString(mCompositionStart).c_str(), mPendingCommitLength,
       ToString(mEditorRect).c_str(),
       ToString(mLastCommitStringTextRectArray).c_str()));
}

bool ContentCacheInParent::HandleQueryContentEvent(
    WidgetQueryContentEvent& aEvent, nsIWidget* aWidget) const {
  MOZ_ASSERT(aWidget);

  // ContentCache doesn't store offset of its start with XP linebreaks.
  // So, we don't support to query contents relative to composition start
  // offset with XP linebreaks.
  if (NS_WARN_IF(!aEvent.mUseNativeLineBreak)) {
    MOZ_LOG(sContentCacheLog, LogLevel::Error,
            ("0x%p HandleQueryContentEvent(), FAILED due to query with XP "
             "linebreaks",
             this));
    return false;
  }

  if (NS_WARN_IF(!aEvent.mInput.IsValidOffset())) {
    MOZ_LOG(
        sContentCacheLog, LogLevel::Error,
        ("0x%p HandleQueryContentEvent(), FAILED due to invalid offset", this));
    return false;
  }

  if (NS_WARN_IF(!aEvent.mInput.IsValidEventMessage(aEvent.mMessage))) {
    MOZ_LOG(
        sContentCacheLog, LogLevel::Error,
        ("0x%p HandleQueryContentEvent(), FAILED due to invalid event message",
         this));
    return false;
  }

  bool isRelativeToInsertionPoint = aEvent.mInput.mRelativeToInsertionPoint;
  if (isRelativeToInsertionPoint) {
    MOZ_LOG(
        sContentCacheLog, LogLevel::Debug,
        ("0x%p HandleQueryContentEvent(), "
         "making offset absolute... aEvent={ mMessage=%s, mInput={ "
         "mOffset=%" PRId64 ", mLength=%" PRIu32 " } }, "
         "WidgetHasComposition()=%s, HasPendingCommit()=%s, "
         "mCompositionStart=%" PRIu32 ", "
         "mPendingCommitLength=%" PRIu32 ", mSelection=%s",
         this, ToChar(aEvent.mMessage), aEvent.mInput.mOffset,
         aEvent.mInput.mLength, GetBoolName(WidgetHasComposition()),
         GetBoolName(HasPendingCommit()), mCompositionStart.valueOr(UINT32_MAX),
         mPendingCommitLength, ToString(mSelection).c_str()));
    if (WidgetHasComposition() || HasPendingCommit()) {
      if (NS_WARN_IF(mCompositionStart.isNothing()) ||
          NS_WARN_IF(!aEvent.mInput.MakeOffsetAbsolute(
              mCompositionStart.value() + mPendingCommitLength))) {
        MOZ_LOG(
            sContentCacheLog, LogLevel::Error,
            ("0x%p HandleQueryContentEvent(), FAILED due to "
             "aEvent.mInput.MakeOffsetAbsolute(mCompositionStart + "
             "mPendingCommitLength) failure, "
             "mCompositionStart=%" PRIu32 ", mPendingCommitLength=%" PRIu32 ", "
             "aEvent={ mMessage=%s, mInput={ mOffset=%" PRId64
             ", mLength=%" PRIu32 " } }",
             this, mCompositionStart.valueOr(UINT32_MAX), mPendingCommitLength,
             ToChar(aEvent.mMessage), aEvent.mInput.mOffset,
             aEvent.mInput.mLength));
        return false;
      }
    } else if (NS_WARN_IF(mSelection.isNothing())) {
      MOZ_LOG(sContentCacheLog, LogLevel::Error,
              ("0x%p HandleQueryContentEvent(), FAILED due to mSelection is "
               "Nothing",
               this));
      return false;
    } else if (NS_WARN_IF(mSelection->mHasRange)) {
      MOZ_LOG(sContentCacheLog, LogLevel::Error,
              ("0x%p HandleQueryContentEvent(), FAILED due to there is no "
               "selection range, but the query requested with relative offset "
               "from selection",
               this));
      return false;
    } else if (NS_WARN_IF(!aEvent.mInput.MakeOffsetAbsolute(
                   mSelection->StartOffset() + mPendingCommitLength))) {
      MOZ_LOG(sContentCacheLog, LogLevel::Error,
              ("0x%p HandleQueryContentEvent(), FAILED due to "
               "aEvent.mInput.MakeOffsetAbsolute(mSelection->StartOffset() + "
               "mPendingCommitLength) failure, mSelection=%s, "
               "mPendingCommitLength=%" PRIu32 ", aEvent={ mMessage=%s, "
               "mInput={ mOffset=%" PRId64 ", mLength=%" PRIu32 " } }",
               this, ToString(mSelection).c_str(), mPendingCommitLength,
               ToChar(aEvent.mMessage), aEvent.mInput.mOffset,
               aEvent.mInput.mLength));
      return false;
    }
  }

  switch (aEvent.mMessage) {
    case eQuerySelectedText:
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
              ("0x%p HandleQueryContentEvent(aEvent={ "
               "mMessage=eQuerySelectedText }, aWidget=0x%p)",
               this, aWidget));
      if (MOZ_UNLIKELY(NS_WARN_IF(mSelection.isNothing()))) {
        // If content cache hasn't been initialized properly, make the query
        // failed.
        MOZ_LOG(sContentCacheLog, LogLevel::Error,
                ("0x%p   HandleQueryContentEvent(), FAILED because mSelection "
                 "is Nothing",
                 this));
        return false;
      }
      MOZ_DIAGNOSTIC_ASSERT(mText.isSome());
      MOZ_DIAGNOSTIC_ASSERT(mSelection->IsValidIn(*mText));
      aEvent.EmplaceReply();
      aEvent.mReply->mFocusedWidget = aWidget;
      if (mSelection->mHasRange) {
        if (MOZ_LIKELY(mText.isSome())) {
          aEvent.mReply->mOffsetAndData.emplace(
              mSelection->StartOffset(),
              Substring(mText.ref(), mSelection->StartOffset(),
                        mSelection->Length()),
              OffsetAndDataFor::SelectedString);
        } else {
          // TODO: Investigate this case.  I find this during
          //       test_mousecapture.xhtml on Linux.
          aEvent.mReply->mOffsetAndData.emplace(
              0u, EmptyString(), OffsetAndDataFor::SelectedString);
        }
      }
      aEvent.mReply->mWritingMode = mSelection->mWritingMode;
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
              ("0x%p   HandleQueryContentEvent(), Succeeded, aEvent={ "
               "mMessage=eQuerySelectedText, mReply=%s }",
               this, ToString(aEvent.mReply).c_str()));
      return true;
    case eQueryTextContent: {
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
              ("0x%p HandleQueryContentEvent(aEvent={ "
               "mMessage=eQueryTextContent, mInput={ mOffset=%" PRId64
               ", mLength=%u } }, aWidget=0x%p), mText->Length()=%zu",
               this, aEvent.mInput.mOffset, aEvent.mInput.mLength, aWidget,
               mText.isSome() ? mText->Length() : 0u));
      if (MOZ_UNLIKELY(NS_WARN_IF(mText.isNothing()))) {
        MOZ_LOG(sContentCacheLog, LogLevel::Error,
                ("0x%p   HandleQueryContentEvent(), FAILED because "
                 "there is no text data",
                 this));
        return false;
      }
      const uint32_t inputOffset = aEvent.mInput.mOffset;
      const uint32_t inputEndOffset = std::min<uint32_t>(
          aEvent.mInput.EndOffset(), mText.isSome() ? mText->Length() : 0u);
      if (MOZ_UNLIKELY(NS_WARN_IF(inputEndOffset < inputOffset))) {
        MOZ_LOG(sContentCacheLog, LogLevel::Error,
                ("0x%p   HandleQueryContentEvent(), FAILED because "
                 "inputOffset=%u is larger than inputEndOffset=%u",
                 this, inputOffset, inputEndOffset));
        return false;
      }
      aEvent.EmplaceReply();
      aEvent.mReply->mFocusedWidget = aWidget;
      const nsAString& textInQueriedRange =
          inputEndOffset > inputOffset
              ? static_cast<const nsAString&>(Substring(
                    mText.ref(), inputOffset, inputEndOffset - inputOffset))
              : static_cast<const nsAString&>(EmptyString());
      aEvent.mReply->mOffsetAndData.emplace(inputOffset, textInQueriedRange,
                                            OffsetAndDataFor::EditorString);
      // TODO: Support font ranges
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
              ("0x%p   HandleQueryContentEvent(), Succeeded, aEvent={ "
               "mMessage=eQueryTextContent, mReply=%s }",
               this, ToString(aEvent.mReply).c_str()));
      return true;
    }
    case eQueryTextRect: {
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
              ("0x%p HandleQueryContentEvent("
               "aEvent={ mMessage=eQueryTextRect, mInput={ mOffset=%" PRId64
               ", mLength=%u } }, aWidget=0x%p), mText->Length()=%zu",
               this, aEvent.mInput.mOffset, aEvent.mInput.mLength, aWidget,
               mText.isSome() ? mText->Length() : 0u));
      // Note that if the query is relative to insertion point, the query was
      // probably requested by native IME.  In such case, we should return
      // non-empty rect since returning failure causes IME showing its window
      // at odd position.
      LayoutDeviceIntRect textRect;
      if (aEvent.mInput.mLength) {
        if (MOZ_UNLIKELY(NS_WARN_IF(
                !GetUnionTextRects(aEvent.mInput.mOffset, aEvent.mInput.mLength,
                                   isRelativeToInsertionPoint, textRect)))) {
          // XXX We don't have cache for this request.
          MOZ_LOG(sContentCacheLog, LogLevel::Error,
                  ("0x%p   HandleQueryContentEvent(), FAILED to get union rect",
                   this));
          return false;
        }
      } else {
        // If the length is 0, we should return caret rect instead.
        if (NS_WARN_IF(!GetCaretRect(aEvent.mInput.mOffset,
                                     isRelativeToInsertionPoint, textRect))) {
          MOZ_LOG(sContentCacheLog, LogLevel::Error,
                  ("0x%p   HandleQueryContentEvent(), FAILED to get caret rect",
                   this));
          return false;
        }
      }
      aEvent.EmplaceReply();
      aEvent.mReply->mFocusedWidget = aWidget;
      aEvent.mReply->mRect = textRect;
      const nsAString& textInQueriedRange =
          mText.isSome() && aEvent.mInput.mOffset <
                                static_cast<int64_t>(
                                    mText.isSome() ? mText->Length() : 0u)
              ? static_cast<const nsAString&>(
                    Substring(mText.ref(), aEvent.mInput.mOffset,
                              mText->Length() >= aEvent.mInput.EndOffset()
                                  ? aEvent.mInput.mLength
                                  : UINT32_MAX))
              : static_cast<const nsAString&>(EmptyString());
      aEvent.mReply->mOffsetAndData.emplace(aEvent.mInput.mOffset,
                                            textInQueriedRange,
                                            OffsetAndDataFor::EditorString);
      // XXX This may be wrong if storing range isn't in the selection range.
      aEvent.mReply->mWritingMode =
          mSelection.isSome() ? mSelection->mWritingMode : WritingMode();
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
              ("0x%p   HandleQueryContentEvent(), Succeeded, aEvent={ "
               "mMessage=eQueryTextRect mReply=%s }",
               this, ToString(aEvent.mReply).c_str()));
      return true;
    }
    case eQueryCaretRect: {
      MOZ_LOG(
          sContentCacheLog, LogLevel::Info,
          ("0x%p HandleQueryContentEvent(aEvent={ mMessage=eQueryCaretRect, "
           "mInput={ mOffset=%" PRId64
           " } }, aWidget=0x%p), mText->Length()=%zu",
           this, aEvent.mInput.mOffset, aWidget,
           mText.isSome() ? mText->Length() : 0u));
      // Note that if the query is relative to insertion point, the query was
      // probably requested by native IME.  In such case, we should return
      // non-empty rect since returning failure causes IME showing its window
      // at odd position.
      LayoutDeviceIntRect caretRect;
      if (NS_WARN_IF(!GetCaretRect(aEvent.mInput.mOffset,
                                   isRelativeToInsertionPoint, caretRect))) {
        MOZ_LOG(sContentCacheLog, LogLevel::Error,
                ("0x%p   HandleQueryContentEvent(),FAILED to get caret rect",
                 this));
        return false;
      }
      aEvent.EmplaceReply();
      aEvent.mReply->mFocusedWidget = aWidget;
      aEvent.mReply->mRect = caretRect;
      aEvent.mReply->mOffsetAndData.emplace(aEvent.mInput.mOffset,
                                            EmptyString(),
                                            OffsetAndDataFor::SelectedString);
      // TODO: Set mWritingMode here
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
              ("0x%p   HandleQueryContentEvent(), Succeeded, aEvent={ "
               "mMessage=eQueryCaretRect, mReply=%s }",
               this, ToString(aEvent.mReply).c_str()));
      return true;
    }
    case eQueryEditorRect:
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
              ("0x%p HandleQueryContentEvent(aEvent={ "
               "mMessage=eQueryEditorRect }, aWidget=0x%p)",
               this, aWidget));
      // XXX This query should fail if no editable elmenet has focus.  Or,
      //     perhaps, should return rect of the window instead.
      aEvent.EmplaceReply();
      aEvent.mReply->mFocusedWidget = aWidget;
      aEvent.mReply->mRect = mEditorRect;
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
              ("0x%p   HandleQueryContentEvent(), Succeeded, aEvent={ "
               "mMessage=eQueryEditorRect, mReply=%s }",
               this, ToString(aEvent.mReply).c_str()));
      return true;
    default:
      aEvent.EmplaceReply();
      aEvent.mReply->mFocusedWidget = aWidget;
      if (NS_WARN_IF(aEvent.Failed())) {
        MOZ_LOG(
            sContentCacheLog, LogLevel::Error,
            ("0x%p HandleQueryContentEvent(), FAILED due to not set enough "
             "data, aEvent={ mMessage=%s, mReply=%s }",
             this, ToChar(aEvent.mMessage), ToString(aEvent.mReply).c_str()));
        return false;
      }
      MOZ_LOG(sContentCacheLog, LogLevel::Info,
              ("0x%p HandleQueryContentEvent(), Succeeded, aEvent={ "
               "mMessage=%s, mReply=%s }",
               this, ToChar(aEvent.mMessage), ToString(aEvent.mReply).c_str()));
      return true;
  }
}

bool ContentCacheInParent::GetTextRect(uint32_t aOffset,
                                       bool aRoundToExistingOffset,
                                       LayoutDeviceIntRect& aTextRect) const {
  MOZ_LOG(
      sContentCacheLog, LogLevel::Info,
      ("0x%p GetTextRect(aOffset=%u, aRoundToExistingOffset=%s), "
       "mTextRectArray=%s, mSelection=%s, mLastCommitStringTextRectArray=%s",
       this, aOffset, GetBoolName(aRoundToExistingOffset),
       ToString(mTextRectArray).c_str(), ToString(mSelection).c_str(),
       ToString(mLastCommitStringTextRectArray).c_str()));

  if (!aOffset) {
    NS_WARNING_ASSERTION(!mFirstCharRect.IsEmpty(), "empty rect");
    aTextRect = mFirstCharRect;
    return !aTextRect.IsEmpty();
  }
  if (mSelection.isSome() && mSelection->mHasRange) {
    if (aOffset == mSelection->mAnchor) {
      NS_WARNING_ASSERTION(
          !mSelection->mAnchorCharRects[eNextCharRect].IsEmpty(), "empty rect");
      aTextRect = mSelection->mAnchorCharRects[eNextCharRect];
      return !aTextRect.IsEmpty();
    }
    if (mSelection->mAnchor && aOffset == mSelection->mAnchor - 1) {
      NS_WARNING_ASSERTION(
          !mSelection->mAnchorCharRects[ePrevCharRect].IsEmpty(), "empty rect");
      aTextRect = mSelection->mAnchorCharRects[ePrevCharRect];
      return !aTextRect.IsEmpty();
    }
    if (aOffset == mSelection->mFocus) {
      NS_WARNING_ASSERTION(
          !mSelection->mFocusCharRects[eNextCharRect].IsEmpty(), "empty rect");
      aTextRect = mSelection->mFocusCharRects[eNextCharRect];
      return !aTextRect.IsEmpty();
    }
    if (mSelection->mFocus && aOffset == mSelection->mFocus - 1) {
      NS_WARNING_ASSERTION(
          !mSelection->mFocusCharRects[ePrevCharRect].IsEmpty(), "empty rect");
      aTextRect = mSelection->mFocusCharRects[ePrevCharRect];
      return !aTextRect.IsEmpty();
    }
  }

  if (mTextRectArray.isSome() && mTextRectArray->IsOffsetInRange(aOffset)) {
    aTextRect = mTextRectArray->GetRect(aOffset);
    return !aTextRect.IsEmpty();
  }

  if (mLastCommitStringTextRectArray.isSome() &&
      mLastCommitStringTextRectArray->IsOffsetInRange(aOffset)) {
    aTextRect = mLastCommitStringTextRectArray->GetRect(aOffset);
    return !aTextRect.IsEmpty();
  }

  if (!aRoundToExistingOffset) {
    aTextRect.SetEmpty();
    return false;
  }

  if (mTextRectArray.isNothing() || !mTextRectArray->HasRects()) {
    // If there are no rects in mTextRectArray, we should refer the start of
    // the selection if there is because IME must query a char rect around it if
    // there is no composition.
    if (mSelection.isNothing()) {
      // Unfortunately, there is no data about text rect...
      aTextRect.SetEmpty();
      return false;
    }
    aTextRect = mSelection->StartCharRect();
    return !aTextRect.IsEmpty();
  }

  // Although we may have mLastCommitStringTextRectArray here and it must have
  // previous character rects at selection.  However, we should stop using it
  // because it's stored really short time after commiting a composition.
  // So, multiple query may return different rect and it may cause flickerling
  // the IME UI.
  uint32_t offset = aOffset;
  if (offset < mTextRectArray->StartOffset()) {
    offset = mTextRectArray->StartOffset();
  } else {
    offset = mTextRectArray->EndOffset() - 1;
  }
  aTextRect = mTextRectArray->GetRect(offset);
  return !aTextRect.IsEmpty();
}

bool ContentCacheInParent::GetUnionTextRects(
    uint32_t aOffset, uint32_t aLength, bool aRoundToExistingOffset,
    LayoutDeviceIntRect& aUnionTextRect) const {
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
          ("0x%p GetUnionTextRects(aOffset=%u, "
           "aLength=%u, aRoundToExistingOffset=%s), mTextRectArray=%s, "
           "mSelection=%s, mLastCommitStringTextRectArray=%s",
           this, aOffset, aLength, GetBoolName(aRoundToExistingOffset),
           ToString(mTextRectArray).c_str(), ToString(mSelection).c_str(),
           ToString(mLastCommitStringTextRectArray).c_str()));

  CheckedInt<uint32_t> endOffset = CheckedInt<uint32_t>(aOffset) + aLength;
  if (!endOffset.isValid()) {
    return false;
  }

  if (mSelection.isSome() && !mSelection->IsCollapsed() &&
      aOffset == mSelection->StartOffset() && aLength == mSelection->Length()) {
    NS_WARNING_ASSERTION(!mSelection->mRect.IsEmpty(), "empty rect");
    aUnionTextRect = mSelection->mRect;
    return !aUnionTextRect.IsEmpty();
  }

  if (aLength == 1) {
    if (!aOffset) {
      NS_WARNING_ASSERTION(!mFirstCharRect.IsEmpty(), "empty rect");
      aUnionTextRect = mFirstCharRect;
      return !aUnionTextRect.IsEmpty();
    }
    if (mSelection.isSome() && mSelection->mHasRange) {
      if (aOffset == mSelection->mAnchor) {
        NS_WARNING_ASSERTION(
            !mSelection->mAnchorCharRects[eNextCharRect].IsEmpty(),
            "empty rect");
        aUnionTextRect = mSelection->mAnchorCharRects[eNextCharRect];
        return !aUnionTextRect.IsEmpty();
      }
      if (mSelection->mAnchor && aOffset == mSelection->mAnchor - 1) {
        NS_WARNING_ASSERTION(
            !mSelection->mAnchorCharRects[ePrevCharRect].IsEmpty(),
            "empty rect");
        aUnionTextRect = mSelection->mAnchorCharRects[ePrevCharRect];
        return !aUnionTextRect.IsEmpty();
      }
      if (aOffset == mSelection->mFocus) {
        NS_WARNING_ASSERTION(
            !mSelection->mFocusCharRects[eNextCharRect].IsEmpty(),
            "empty rect");
        aUnionTextRect = mSelection->mFocusCharRects[eNextCharRect];
        return !aUnionTextRect.IsEmpty();
      }
      if (mSelection->mFocus && aOffset == mSelection->mFocus - 1) {
        NS_WARNING_ASSERTION(
            !mSelection->mFocusCharRects[ePrevCharRect].IsEmpty(),
            "empty rect");
        aUnionTextRect = mSelection->mFocusCharRects[ePrevCharRect];
        return !aUnionTextRect.IsEmpty();
      }
    }
  }

  // Even if some text rects are not cached of the queried range,
  // we should return union rect when the first character's rect is cached
  // since the first character rect is important and the others are not so
  // in most cases.

  if (!aOffset && mSelection.isSome() && mSelection->mHasRange &&
      aOffset != mSelection->mAnchor && aOffset != mSelection->mFocus &&
      (mTextRectArray.isNothing() ||
       !mTextRectArray->IsOffsetInRange(aOffset)) &&
      (mLastCommitStringTextRectArray.isNothing() ||
       !mLastCommitStringTextRectArray->IsOffsetInRange(aOffset))) {
    // The first character rect isn't cached.
    return false;
  }

  // Use mLastCommitStringTextRectArray only when it overlaps with aOffset
  // even if aROundToExistingOffset is true for avoiding flickerling IME UI.
  // See the last comment in GetTextRect() for the detail.
  if (mLastCommitStringTextRectArray.isSome() &&
      mLastCommitStringTextRectArray->IsOverlappingWith(aOffset, aLength)) {
    aUnionTextRect =
        mLastCommitStringTextRectArray->GetUnionRectAsFarAsPossible(
            aOffset, aLength, aRoundToExistingOffset);
  } else {
    aUnionTextRect.SetEmpty();
  }

  if (mTextRectArray.isSome() &&
      ((aRoundToExistingOffset && mTextRectArray->HasRects()) ||
       mTextRectArray->IsOverlappingWith(aOffset, aLength))) {
    aUnionTextRect =
        aUnionTextRect.Union(mTextRectArray->GetUnionRectAsFarAsPossible(
            aOffset, aLength, aRoundToExistingOffset));
  }

  if (!aOffset) {
    aUnionTextRect = aUnionTextRect.Union(mFirstCharRect);
  }
  if (mSelection.isSome() && mSelection->mHasRange) {
    if (aOffset <= mSelection->mAnchor &&
        mSelection->mAnchor < endOffset.value()) {
      aUnionTextRect =
          aUnionTextRect.Union(mSelection->mAnchorCharRects[eNextCharRect]);
    }
    if (mSelection->mAnchor && aOffset <= mSelection->mAnchor - 1 &&
        mSelection->mAnchor - 1 < endOffset.value()) {
      aUnionTextRect =
          aUnionTextRect.Union(mSelection->mAnchorCharRects[ePrevCharRect]);
    }
    if (aOffset <= mSelection->mFocus &&
        mSelection->mFocus < endOffset.value()) {
      aUnionTextRect =
          aUnionTextRect.Union(mSelection->mFocusCharRects[eNextCharRect]);
    }
    if (mSelection->mFocus && aOffset <= mSelection->mFocus - 1 &&
        mSelection->mFocus - 1 < endOffset.value()) {
      aUnionTextRect =
          aUnionTextRect.Union(mSelection->mFocusCharRects[ePrevCharRect]);
    }
  }

  return !aUnionTextRect.IsEmpty();
}

bool ContentCacheInParent::GetCaretRect(uint32_t aOffset,
                                        bool aRoundToExistingOffset,
                                        LayoutDeviceIntRect& aCaretRect) const {
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
          ("0x%p GetCaretRect(aOffset=%u, aRoundToExistingOffset=%s), "
           "mCaret=%s, mTextRectArray=%s, mSelection=%s, mFirstCharRect=%s",
           this, aOffset, GetBoolName(aRoundToExistingOffset),
           ToString(mCaret).c_str(), ToString(mTextRectArray).c_str(),
           ToString(mSelection).c_str(), ToString(mFirstCharRect).c_str()));

  if (mCaret.isSome() && mCaret->mOffset == aOffset) {
    aCaretRect = mCaret->mRect;
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

    if (mSelection.isSome() && mSelection->mWritingMode.IsVertical()) {
      aCaretRect.MoveToY(aCaretRect.YMost());
    } else {
      // XXX bidi-unaware.
      aCaretRect.MoveToX(aCaretRect.XMost());
    }
  }

  // XXX This is not bidi aware because we don't cache each character's
  //     direction.  However, this is usually used by IME, so, assuming the
  //     character is in LRT context must not cause any problem.
  if (mSelection.isSome() && mSelection->mWritingMode.IsVertical()) {
    aCaretRect.SetHeight(mCaret.isSome() ? mCaret->mRect.Height() : 1);
  } else {
    aCaretRect.SetWidth(mCaret.isSome() ? mCaret->mRect.Width() : 1);
  }
  return true;
}

bool ContentCacheInParent::OnCompositionEvent(
    const WidgetCompositionEvent& aCompositionEvent) {
  MOZ_LOG(
      sContentCacheLog, LogLevel::Info,
      ("0x%p OnCompositionEvent(aCompositionEvent={ "
       "mMessage=%s, mData=\"%s\", mRanges->Length()=%zu }), "
       "PendingEventsNeedingAck()=%u, WidgetHasComposition()=%s, "
       "mHandlingCompositions.Length()=%zu, HasPendingCommit()=%s, "
       "mIsChildIgnoringCompositionEvents=%s, mCommitStringByRequest=0x%p",
       this, ToChar(aCompositionEvent.mMessage),
       PrintStringDetail(aCompositionEvent.mData,
                         PrintStringDetail::kMaxLengthForCompositionString)
           .get(),
       aCompositionEvent.mRanges ? aCompositionEvent.mRanges->Length() : 0,
       PendingEventsNeedingAck(), GetBoolName(WidgetHasComposition()),
       mHandlingCompositions.Length(), GetBoolName(HasPendingCommit()),
       GetBoolName(mIsChildIgnoringCompositionEvents), mCommitStringByRequest));

#if MOZ_DIAGNOSTIC_ASSERT_ENABLED
  mDispatchedEventMessages.AppendElement(aCompositionEvent.mMessage);
#endif  // #ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED

  // We must be able to simulate the selection because
  // we might not receive selection updates in time
  if (!WidgetHasComposition()) {
    if (mCompositionStartInChild.isSome()) {
      // If there is pending composition in the remote process, let's use
      // its start offset temporarily because this stores a lot of information
      // around it and the user must look around there, so, showing some UI
      // around it must make sense.
      mCompositionStart = mCompositionStartInChild;
    } else {
      mCompositionStart = Some(mSelection.isSome() && mSelection->mHasRange
                                   ? mSelection->StartOffset()
                                   : 0u);
    }
    MOZ_ASSERT(aCompositionEvent.mMessage == eCompositionStart);
    mHandlingCompositions.AppendElement(
        HandlingCompositionData(aCompositionEvent.mCompositionId));
  }

  mHandlingCompositions.LastElement().mSentCommitEvent =
      aCompositionEvent.CausesDOMCompositionEndEvent();
  MOZ_ASSERT(mHandlingCompositions.LastElement().mCompositionId ==
             aCompositionEvent.mCompositionId);

  if (!WidgetHasComposition()) {
    // mCompositionStart will be reset when commit event is completely handled
    // in the remote process.
    if (mHandlingCompositions.Length() == 1u) {
      mPendingCommitLength = aCompositionEvent.mData.Length();
    }
    MOZ_ASSERT(HasPendingCommit());
  } else if (aCompositionEvent.mMessage != eCompositionStart) {
    mHandlingCompositions.LastElement().mCompositionString =
        aCompositionEvent.mData;
  }

  // During REQUEST_TO_COMMIT_COMPOSITION or REQUEST_TO_CANCEL_COMPOSITION,
  // widget usually sends a eCompositionChange and/or eCompositionCommit event
  // to finalize or clear the composition, respectively.  In this time,
  // we need to intercept all composition events here and pass the commit
  // string for returning to the remote process as a result of
  // RequestIMEToCommitComposition().  Then, eCommitComposition event will
  // be dispatched with the committed string in the remote process internally.
  if (mCommitStringByRequest) {
    if (aCompositionEvent.mMessage == eCompositionCommitAsIs) {
      *mCommitStringByRequest =
          mHandlingCompositions.LastElement().mCompositionString;
    } else {
      MOZ_ASSERT(aCompositionEvent.mMessage == eCompositionChange ||
                 aCompositionEvent.mMessage == eCompositionCommit);
      *mCommitStringByRequest = aCompositionEvent.mData;
    }
    // We need to wait eCompositionCommitRequestHandled from the remote process
    // in this case.  Therefore, mPendingEventsNeedingAck needs to be
    // incremented here.
    if (!WidgetHasComposition()) {
      mHandlingCompositions.LastElement().mPendingEventsNeedingAck++;
    }
    return false;
  }

  mHandlingCompositions.LastElement().mPendingEventsNeedingAck++;
  return true;
}

void ContentCacheInParent::OnSelectionEvent(
    const WidgetSelectionEvent& aSelectionEvent) {
  MOZ_LOG(sContentCacheLog, LogLevel::Info,
          ("0x%p OnSelectionEvent(aEvent={ "
           "mMessage=%s, mOffset=%u, mLength=%u, mReversed=%s, "
           "mExpandToClusterBoundary=%s, mUseNativeLineBreak=%s }), "
           "PendingEventsNeedingAck()=%u, WidgetHasComposition()=%s, "
           "mHandlingCompositions.Length()=%zu, HasPendingCommit()=%s, "
           "mIsChildIgnoringCompositionEvents=%s",
           this, ToChar(aSelectionEvent.mMessage), aSelectionEvent.mOffset,
           aSelectionEvent.mLength, GetBoolName(aSelectionEvent.mReversed),
           GetBoolName(aSelectionEvent.mExpandToClusterBoundary),
           GetBoolName(aSelectionEvent.mUseNativeLineBreak),
           PendingEventsNeedingAck(), GetBoolName(WidgetHasComposition()),
           mHandlingCompositions.Length(), GetBoolName(HasPendingCommit()),
           GetBoolName(mIsChildIgnoringCompositionEvents)));

#if MOZ_DIAGNOSTIC_ASSERT_ENABLED && !defined(FUZZING_SNAPSHOT)
  mDispatchedEventMessages.AppendElement(aSelectionEvent.mMessage);
#endif  // MOZ_DIAGNOSTIC_ASSERT_ENABLED

  mPendingSetSelectionEventNeedingAck++;
}

void ContentCacheInParent::OnEventNeedingAckHandled(nsIWidget* aWidget,
                                                    EventMessage aMessage,
                                                    uint32_t aCompositionId) {
  // This is called when the child process receives WidgetCompositionEvent or
  // WidgetSelectionEvent.

  HandlingCompositionData* handlingCompositionData =
      aMessage != eSetSelection ? GetHandlingCompositionData(aCompositionId)
                                : nullptr;

  MOZ_LOG(sContentCacheLog, LogLevel::Info,
          ("0x%p OnEventNeedingAckHandled(aWidget=0x%p, aMessage=%s, "
           "aCompositionId=%" PRIu32
           "), PendingEventsNeedingAck()=%u, WidgetHasComposition()=%s, "
           "mHandlingCompositions.Length()=%zu, HasPendingCommit()=%s, "
           "mIsChildIgnoringCompositionEvents=%s, handlingCompositionData=0x%p",
           this, aWidget, ToChar(aMessage), aCompositionId,
           PendingEventsNeedingAck(), GetBoolName(WidgetHasComposition()),
           mHandlingCompositions.Length(), GetBoolName(HasPendingCommit()),
           GetBoolName(mIsChildIgnoringCompositionEvents),
           handlingCompositionData));

  // If we receive composition event messages for older one or invalid one,
  // we should ignore them.
  if (NS_WARN_IF(aMessage != eSetSelection && !handlingCompositionData)) {
    return;
  }

#if MOZ_DIAGNOSTIC_ASSERT_ENABLED && !defined(FUZZING_SNAPSHOT)
  mReceivedEventMessages.AppendElement(aMessage);
#endif  // #if MOZ_DIAGNOSTIC_ASSERT_ENABLED

  const bool isCommittedInChild =
      // Commit requester in the remote process has committed the composition.
      aMessage == eCompositionCommitRequestHandled ||
      // The commit event has been handled normally in the remote process.
      (!mIsChildIgnoringCompositionEvents &&
       WidgetCompositionEvent::IsFollowedByCompositionEnd(aMessage));
  const bool hasPendingCommit = HasPendingCommit();

  if (isCommittedInChild) {
#if MOZ_DIAGNOSTIC_ASSERT_ENABLED && !defined(FUZZING_SNAPSHOT)
    if (mHandlingCompositions.Length() == 1u) {
      RemoveUnnecessaryEventMessageLog();
    }

    if (NS_WARN_IF(aMessage != eCompositionCommitRequestHandled &&
                   !handlingCompositionData->mSentCommitEvent)) {
      nsPrintfCString info(
          "\nReceived unexpected commit event message (%s) which we've "
          "not sent yet\n\n",
          ToChar(aMessage));
      AppendEventMessageLog(info);
      CrashReporter::AppendAppNotesToCrashReport(info);
      MOZ_DIAGNOSTIC_ASSERT(
          false,
          "Received unexpected commit event which has not been sent yet");
    }
#endif  // #if MOZ_DIAGNOSTIC_ASSERT_ENABLED

    // This should not occur, though.  If we receive a commit notification for
    // not the oldest composition, we should forget all older compositions.
    size_t numberOfOutdatedCompositions = 1u;
    for (auto& data : mHandlingCompositions) {
      if (&data == handlingCompositionData) {
        if (
            // Don't put the info into the log when we've already sent commit
            // event because it may be just inserting a character without
            // composing state, but the remote process may move focus at
            // eCompositionStart.  This may happen with UI of IME to put only
            // one character, e.g., the default Emoji picker of Windows.
            !data.mSentCommitEvent &&
            // In the normal case, only one message should remain, however,
            // remaining 2 or more messages is also valid, for example, the
            // remote process may have a composition update listener which
            // takes a while.  Then, we can have multiple pending messages.
            data.mPendingEventsNeedingAck >= 1u) {
          MOZ_LOG(
              sContentCacheLog, LogLevel::Debug,
              ("    NOTE: BrowserParent has %" PRIu32
               " pending composition messages for the handling composition, "
               "but before they are handled in the remote process, the active "
               "composition is commited by a request.  "
               "OnEventNeedingAckHandled() calls for them will be ignored",
               data.mPendingEventsNeedingAck));
        }
        break;
      }
      if (MOZ_UNLIKELY(data.mPendingEventsNeedingAck)) {
        MOZ_LOG(sContentCacheLog, LogLevel::Warning,
                ("    BrowserParent has %" PRIu32
                 " pending composition messages for an older composition than "
                 "the handling composition, but it'll be removed because newer "
                 "composition gets comitted in the remote process",
                 data.mPendingEventsNeedingAck));
      }
      numberOfOutdatedCompositions++;
    }
    mHandlingCompositions.RemoveElementsAt(0u, numberOfOutdatedCompositions);
    handlingCompositionData = nullptr;

    // Forget pending commit string length if it's handled in the remote
    // process.  Note that this doesn't care too old composition's commit
    // string because in such case, we cannot return proper information
    // to IME synchronously.
    mPendingCommitLength = 0;
  }

  if (WidgetCompositionEvent::IsFollowedByCompositionEnd(aMessage)) {
    // After the remote process receives eCompositionCommit(AsIs) event,
    // it'll restart to handle composition events.
    mIsChildIgnoringCompositionEvents = false;

#if MOZ_DIAGNOSTIC_ASSERT_ENABLED && !defined(FUZZING_SNAPSHOT)
    if (NS_WARN_IF(!hasPendingCommit)) {
      nsPrintfCString info(
          "\nThere is no pending comment events but received "
          "%s message from the remote child\n\n",
          ToChar(aMessage));
      AppendEventMessageLog(info);
      CrashReporter::AppendAppNotesToCrashReport(info);
      MOZ_DIAGNOSTIC_ASSERT(
          false,
          "No pending commit events but received unexpected commit event");
    }
#endif  // #if MOZ_DIAGNOSTIC_ASSERT_ENABLED
  } else if (aMessage == eCompositionCommitRequestHandled && hasPendingCommit) {
    // If the remote process commits composition synchronously after
    // requesting commit composition and we've already sent commit composition,
    // it starts to ignore following composition events until receiving
    // eCompositionStart event.
    mIsChildIgnoringCompositionEvents = true;
  }

  // If neither widget (i.e., IME) nor the remote process has composition,
  // now, we can forget composition string informations.
  if (mHandlingCompositions.IsEmpty()) {
    mCompositionStart.reset();
  }

  if (handlingCompositionData) {
    if (NS_WARN_IF(!handlingCompositionData->mPendingEventsNeedingAck)) {
#if MOZ_DIAGNOSTIC_ASSERT_ENABLED && !defined(FUZZING_SNAPSHOT)
      nsPrintfCString info(
          "\nThere is no pending events but received %s "
          "message from the remote child\n\n",
          ToChar(aMessage));
      AppendEventMessageLog(info);
      CrashReporter::AppendAppNotesToCrashReport(info);
      MOZ_DIAGNOSTIC_ASSERT(
          false, "No pending event message but received unexpected event");
#endif  // #if MOZ_DIAGNOSTIC_ASSERT_ENABLED
    } else {
      handlingCompositionData->mPendingEventsNeedingAck--;
    }
  } else if (aMessage == eSetSelection) {
    if (NS_WARN_IF(!mPendingSetSelectionEventNeedingAck)) {
#if MOZ_DIAGNOSTIC_ASSERT_ENABLED && !defined(FUZZING_SNAPSHOT)
      nsAutoCString info(
          "\nThere is no pending set selection events but received from the "
          "remote child\n\n");
      AppendEventMessageLog(info);
      CrashReporter::AppendAppNotesToCrashReport(info);
      MOZ_DIAGNOSTIC_ASSERT(
          false, "No pending event message but received unexpected event");
#endif  // #if MOZ_DIAGNOSTIC_ASSERT_ENABLED
    } else {
      mPendingSetSelectionEventNeedingAck--;
    }
  }

  if (!PendingEventsNeedingAck()) {
    FlushPendingNotifications(aWidget);
  }
}

bool ContentCacheInParent::RequestIMEToCommitComposition(
    nsIWidget* aWidget, bool aCancel, uint32_t aCompositionId,
    nsAString& aCommittedString) {
  HandlingCompositionData* const handlingCompositionData =
      GetHandlingCompositionData(aCompositionId);

  MOZ_LOG(
      sContentCacheLog, LogLevel::Info,
      ("0x%p RequestToCommitComposition(aWidget=%p, "
       "aCancel=%s, aCompositionId=%" PRIu32
       "), mHandlingCompositions.Length()=%zu, HasPendingCommit()=%s, "
       "mIsChildIgnoringCompositionEvents=%s, "
       "IMEStateManager::DoesBrowserParentHaveIMEFocus(&mBrowserParent)=%s, "
       "WidgetHasComposition()=%s, mCommitStringByRequest=%p, "
       "handlingCompositionData=0x%p",
       this, aWidget, GetBoolName(aCancel), aCompositionId,
       mHandlingCompositions.Length(), GetBoolName(HasPendingCommit()),
       GetBoolName(mIsChildIgnoringCompositionEvents),
       GetBoolName(
           IMEStateManager::DoesBrowserParentHaveIMEFocus(&mBrowserParent)),
       GetBoolName(WidgetHasComposition()), mCommitStringByRequest,
       handlingCompositionData));

  MOZ_ASSERT(!mCommitStringByRequest);

  // If we don't know the composition ID, it must have already been committed
  // in this process.  In the case, we should do nothing here.
  if (NS_WARN_IF(!handlingCompositionData)) {
#if MOZ_DIAGNOSTIC_ASSERT_ENABLED
    mRequestIMEToCommitCompositionResults.AppendElement(
        RequestIMEToCommitCompositionResult::eToUnknownCompositionReceived);
#endif  // #if MOZ_DIAGNOSTIC_ASSERT_ENABLED
    return false;
  }

  // If we receive a commit result for not latest composition, this request is
  // too late for IME.  The remote process should wait following composition
  // events for cleaning up TextComposition and handle the request as it's
  // handled asynchronously.
  if (handlingCompositionData != &mHandlingCompositions.LastElement()) {
#if MOZ_DIAGNOSTIC_ASSERT_ENABLED
    mRequestIMEToCommitCompositionResults.AppendElement(
        RequestIMEToCommitCompositionResult::eToOldCompositionReceived);
#endif  // #if MOZ_DIAGNOSTIC_ASSERT_ENABLED
    return false;
  }

  // If the composition has already been commit, th remote process will receive
  // composition events and clean up TextComposition.  So, this should do
  // nothing and TextComposition should handle the request as it's handled
  // asynchronously.
  // XXX Perhaps, this is wrong because TextComposition in child process
  //     may commit the composition with current composition string in the
  //     remote process.  I.e., it may be different from actual commit string
  //     which user typed.  So, perhaps, we should return true and the commit
  //     string.
  if (handlingCompositionData->mSentCommitEvent) {
#if MOZ_DIAGNOSTIC_ASSERT_ENABLED
    mRequestIMEToCommitCompositionResults.AppendElement(
        RequestIMEToCommitCompositionResult::eToCommittedCompositionReceived);
#endif  // #if MOZ_DIAGNOSTIC_ASSERT_ENABLED
    return false;
  }

  // If BrowserParent which has IME focus was already changed to different one,
  // the request shouldn't be sent to IME because it's too late.
  if (!IMEStateManager::DoesBrowserParentHaveIMEFocus(&mBrowserParent)) {
    // Use the latest composition string which may not be handled in the
    // remote process for avoiding data loss.
#if MOZ_DIAGNOSTIC_ASSERT_ENABLED
    mRequestIMEToCommitCompositionResults.AppendElement(
        RequestIMEToCommitCompositionResult::eReceivedAfterBrowserParentBlur);
#endif  // #if MOZ_DIAGNOSTIC_ASSERT_ENABLED
    aCommittedString = handlingCompositionData->mCompositionString;
    // After we return true from here, i.e., without actually requesting IME
    // to commit composition, we will receive eCompositionCommitRequestHandled
    // pseudo event message from the remote process.  So, we need to increment
    // mPendingEventsNeedingAck here.
    handlingCompositionData->mPendingEventsNeedingAck++;
    return true;
  }

  RefPtr<TextComposition> composition =
      IMEStateManager::GetTextCompositionFor(aWidget);
  if (NS_WARN_IF(!composition)) {
    MOZ_LOG(sContentCacheLog, LogLevel::Warning,
            ("  0x%p RequestToCommitComposition(), "
             "does nothing due to no composition",
             this));
#if MOZ_DIAGNOSTIC_ASSERT_ENABLED
    mRequestIMEToCommitCompositionResults.AppendElement(
        RequestIMEToCommitCompositionResult::eReceivedButNoTextComposition);
#endif  // #if MOZ_DIAGNOSTIC_ASSERT_ENABLED
    return false;
  }

  // If we receive a request for different composition, we must have already
  // sent a commit event. So the remote process should handle it.
  // XXX I think that this should never happen because we already checked
  // whether handlingCompositionData is the latest composition or not.
  // However, we don't want to commit different composition for the users.
  // Therefore, let's handle the odd case here.
  if (NS_WARN_IF(composition->Id() != aCompositionId)) {
#if MOZ_DIAGNOSTIC_ASSERT_ENABLED
    mRequestIMEToCommitCompositionResults.AppendElement(
        RequestIMEToCommitCompositionResult::
            eReceivedButForDifferentTextComposition);
#endif  // #if MOZ_DIAGNOSTIC_ASSERT_ENABLED
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

  MOZ_LOG(
      sContentCacheLog, LogLevel::Info,
      ("  0x%p RequestToCommitComposition(), "
       "WidgetHasComposition()=%s, the composition %s committed synchronously",
       this, GetBoolName(WidgetHasComposition()),
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
    mRequestIMEToCommitCompositionResults.AppendElement(
        RequestIMEToCommitCompositionResult::eHandledAsynchronously);
#endif  // #if MOZ_DIAGNOSTIC_ASSERT_ENABLED
    return false;
  }

  // When the composition is committed synchronously, the commit string will be
  // returned to the remote process.  Then, PuppetWidget will dispatch
  // eCompositionCommit event with the returned commit string (i.e., the value
  // is aCommittedString of this method) and that causes destroying
  // TextComposition instance in the remote process (Note that TextComposition
  // instance in this process was already destroyed).
#if MOZ_DIAGNOSTIC_ASSERT_ENABLED
  mRequestIMEToCommitCompositionResults.AppendElement(
      RequestIMEToCommitCompositionResult::eHandledSynchronously);
#endif  // #if MOZ_DIAGNOSTIC_ASSERT_ENABLED
  return true;
}

void ContentCacheInParent::MaybeNotifyIME(
    nsIWidget* aWidget, const IMENotification& aNotification) {
  if (!PendingEventsNeedingAck()) {
    IMEStateManager::NotifyIME(aNotification, aWidget, &mBrowserParent);
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

void ContentCacheInParent::FlushPendingNotifications(nsIWidget* aWidget) {
  MOZ_ASSERT(!PendingEventsNeedingAck());

  // If the BrowserParent's widget has already gone, this can do nothing since
  // widget is necessary to notify IME of something.
  if (!aWidget) {
    return;
  }

  // New notifications which are notified during flushing pending notifications
  // should be merged again.
  const bool pendingEventNeedingAckIncremented =
      !mHandlingCompositions.IsEmpty();
  if (pendingEventNeedingAckIncremented) {
    mHandlingCompositions.LastElement().mPendingEventsNeedingAck++;
  }

  nsCOMPtr<nsIWidget> widget = aWidget;

  // First, text change notification should be sent because selection change
  // notification notifies IME of current selection range in the latest content.
  // So, IME may need the latest content before that.
  if (mPendingTextChange.HasNotification()) {
    IMENotification notification(mPendingTextChange);
    if (!widget->Destroyed()) {
      mPendingTextChange.Clear();
      IMEStateManager::NotifyIME(notification, widget, &mBrowserParent);
    }
  }

  if (mPendingSelectionChange.HasNotification()) {
    IMENotification notification(mPendingSelectionChange);
    if (!widget->Destroyed()) {
      mPendingSelectionChange.Clear();
      IMEStateManager::NotifyIME(notification, widget, &mBrowserParent);
    }
  }

  // Layout change notification should be notified after selection change
  // notification because IME may want to query position of new caret position.
  if (mPendingLayoutChange.HasNotification()) {
    IMENotification notification(mPendingLayoutChange);
    if (!widget->Destroyed()) {
      mPendingLayoutChange.Clear();
      IMEStateManager::NotifyIME(notification, widget, &mBrowserParent);
    }
  }

  // Finally, send composition update notification because it notifies IME of
  // finishing handling whole sending events.
  if (mPendingCompositionUpdate.HasNotification()) {
    IMENotification notification(mPendingCompositionUpdate);
    if (!widget->Destroyed()) {
      mPendingCompositionUpdate.Clear();
      IMEStateManager::NotifyIME(notification, widget, &mBrowserParent);
    }
  }

  // Decrement it which was incremented above.
  if (!mHandlingCompositions.IsEmpty() && pendingEventNeedingAckIncremented &&
      mHandlingCompositions.LastElement().mPendingEventsNeedingAck) {
    mHandlingCompositions.LastElement().mPendingEventsNeedingAck--;
  }

  if (!PendingEventsNeedingAck() && !widget->Destroyed() &&
      (mPendingTextChange.HasNotification() ||
       mPendingSelectionChange.HasNotification() ||
       mPendingLayoutChange.HasNotification() ||
       mPendingCompositionUpdate.HasNotification())) {
    FlushPendingNotifications(widget);
  }
}

#if MOZ_DIAGNOSTIC_ASSERT_ENABLED

void ContentCacheInParent::RemoveUnnecessaryEventMessageLog() {
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
    for (size_t i = mRequestIMEToCommitCompositionResults.Length(); i > 1;
         i--) {
      if (mRequestIMEToCommitCompositionResults[i - 1] ==
              RequestIMEToCommitCompositionResult::
                  eReceivedAfterBrowserParentBlur ||
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

void ContentCacheInParent::AppendEventMessageLog(nsACString& aLog) const {
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

#endif  // #if MOZ_DIAGNOSTIC_ASSERT_ENABLED

/*****************************************************************************
 * mozilla::ContentCache::Selection
 *****************************************************************************/

ContentCache::Selection::Selection(
    const WidgetQueryContentEvent& aQuerySelectedTextEvent)
    : mAnchor(UINT32_MAX),
      mFocus(UINT32_MAX),
      mWritingMode(aQuerySelectedTextEvent.mReply->WritingModeRef()),
      mHasRange(aQuerySelectedTextEvent.mReply->mOffsetAndData.isSome()) {
  MOZ_ASSERT(aQuerySelectedTextEvent.mMessage == eQuerySelectedText);
  MOZ_ASSERT(aQuerySelectedTextEvent.Succeeded());
  if (mHasRange) {
    mAnchor = aQuerySelectedTextEvent.mReply->AnchorOffset();
    mFocus = aQuerySelectedTextEvent.mReply->FocusOffset();
  }
}

/*****************************************************************************
 * mozilla::ContentCache::TextRectArray
 *****************************************************************************/

LayoutDeviceIntRect ContentCache::TextRectArray::GetRect(
    uint32_t aOffset) const {
  LayoutDeviceIntRect rect;
  if (IsOffsetInRange(aOffset)) {
    rect = mRects[aOffset - mStart];
  }
  return rect;
}

LayoutDeviceIntRect ContentCache::TextRectArray::GetUnionRect(
    uint32_t aOffset, uint32_t aLength) const {
  LayoutDeviceIntRect rect;
  if (!IsRangeCompletelyInRange(aOffset, aLength)) {
    return rect;
  }
  for (uint32_t i = 0; i < aLength; i++) {
    rect = rect.Union(mRects[aOffset - mStart + i]);
  }
  return rect;
}

LayoutDeviceIntRect ContentCache::TextRectArray::GetUnionRectAsFarAsPossible(
    uint32_t aOffset, uint32_t aLength, bool aRoundToExistingOffset) const {
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

}  // namespace mozilla
