/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TextEvents.h"
#include "mozilla/TextEventDispatcher.h"
#include "nsIDocShell.h"
#include "nsIFrame.h"
#include "nsIPresShell.h"
#include "nsIWidget.h"
#include "nsPIDOMWindow.h"
#include "nsView.h"

namespace mozilla {
namespace widget {

/******************************************************************************
 * TextEventDispatcher
 *****************************************************************************/

TextEventDispatcher::TextEventDispatcher(nsIWidget* aWidget)
  : mWidget(aWidget)
  , mInitialized(false)
  , mForTests(false)
{
  MOZ_RELEASE_ASSERT(mWidget, "aWidget must not be nullptr");
}

nsresult
TextEventDispatcher::Init()
{
  if (mInitialized) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }
  mInitialized = true;
  mForTests = false;
  return NS_OK;
}

nsresult
TextEventDispatcher::InitForTests()
{
  if (mInitialized) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }
  mInitialized = true;
  mForTests = true;
  return NS_OK;
}

void
TextEventDispatcher::OnDestroyWidget()
{
  mWidget = nullptr;
  mPendingComposition.Clear();
}

nsresult
TextEventDispatcher::GetState() const
{
  if (!mInitialized) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  if (!mWidget || mWidget->Destroyed()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return NS_OK;
}

void
TextEventDispatcher::InitEvent(WidgetCompositionEvent& aEvent) const
{
  aEvent.time = PR_IntervalNow();
  aEvent.mFlags.mIsSynthesizedForTests = mForTests;
}

nsresult
TextEventDispatcher::StartComposition(nsEventStatus& aStatus)
{
  aStatus = nsEventStatus_eIgnore;

  nsresult rv = GetState();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIWidget> widget(mWidget);
  WidgetCompositionEvent compositionStartEvent(true, NS_COMPOSITION_START,
                                               widget);
  InitEvent(compositionStartEvent);
  rv = widget->DispatchEvent(&compositionStartEvent, aStatus);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
TextEventDispatcher::CommitComposition(nsEventStatus& aStatus,
                                       const nsAString* aCommitString)
{
  aStatus = nsEventStatus_eIgnore;

  nsresult rv = GetState();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIWidget> widget(mWidget);
  uint32_t message = aCommitString ? NS_COMPOSITION_COMMIT :
                                     NS_COMPOSITION_COMMIT_AS_IS;
  WidgetCompositionEvent compositionCommitEvent(true, message, widget);
  InitEvent(compositionCommitEvent);
  if (message == NS_COMPOSITION_COMMIT) {
    compositionCommitEvent.mData = *aCommitString;
  }
  rv = widget->DispatchEvent(&compositionCommitEvent, aStatus);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

/******************************************************************************
 * TextEventDispatcher::PendingComposition
 *****************************************************************************/

TextEventDispatcher::PendingComposition::PendingComposition()
{
  Clear();
}

void
TextEventDispatcher::PendingComposition::Clear()
{
  mString.Truncate();
  mClauses = nullptr;
  mCaret.mRangeType = 0;
}

void
TextEventDispatcher::PendingComposition::EnsureClauseArray()
{
  if (mClauses) {
    return;
  }
  mClauses = new TextRangeArray();
}

nsresult
TextEventDispatcher::PendingComposition::SetString(const nsAString& aString)
{
  mString = aString;
  return NS_OK;
}

nsresult
TextEventDispatcher::PendingComposition::AppendClause(uint32_t aLength,
                                                      uint32_t aAttribute)
{
  if (NS_WARN_IF(!aLength)) {
    return NS_ERROR_INVALID_ARG;
  }

  switch (aAttribute) {
    case NS_TEXTRANGE_RAWINPUT:
    case NS_TEXTRANGE_SELECTEDRAWTEXT:
    case NS_TEXTRANGE_CONVERTEDTEXT:
    case NS_TEXTRANGE_SELECTEDCONVERTEDTEXT: {
      EnsureClauseArray();
      TextRange textRange;
      textRange.mStartOffset =
        mClauses->IsEmpty() ? 0 : mClauses->LastElement().mEndOffset;
      textRange.mEndOffset = textRange.mStartOffset + aLength;
      textRange.mRangeType = aAttribute;
      mClauses->AppendElement(textRange);
      return NS_OK;
    }
    default:
      return NS_ERROR_INVALID_ARG;
  }
}

nsresult
TextEventDispatcher::PendingComposition::SetCaret(uint32_t aOffset,
                                                  uint32_t aLength)
{
  mCaret.mStartOffset = aOffset;
  mCaret.mEndOffset = mCaret.mStartOffset + aLength;
  mCaret.mRangeType = NS_TEXTRANGE_CARETPOSITION;
  return NS_OK;
}

nsresult
TextEventDispatcher::PendingComposition::Flush(
                       const TextEventDispatcher* aDispatcher,
                       nsEventStatus& aStatus)
{
  aStatus = nsEventStatus_eIgnore;

  nsresult rv = aDispatcher->GetState();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (mClauses && !mClauses->IsEmpty() &&
      mClauses->LastElement().mEndOffset != mString.Length()) {
    NS_WARNING("Sum of length of the all clauses must be same as the string "
               "length");
    Clear();
    return NS_ERROR_ILLEGAL_VALUE;
  }
  if (mCaret.mRangeType == NS_TEXTRANGE_CARETPOSITION) {
    if (mCaret.mEndOffset > mString.Length()) {
      NS_WARNING("Caret position is out of the composition string");
      Clear();
      return NS_ERROR_ILLEGAL_VALUE;
    }
    EnsureClauseArray();
    mClauses->AppendElement(mCaret);
  }

  nsCOMPtr<nsIWidget> widget(aDispatcher->mWidget);
  WidgetCompositionEvent compChangeEvent(true, NS_COMPOSITION_CHANGE, widget);
  aDispatcher->InitEvent(compChangeEvent);
  compChangeEvent.mData = mString;
  if (mClauses) {
    MOZ_ASSERT(!mClauses->IsEmpty(),
               "mClauses must be non-empty array when it's not nullptr");
    compChangeEvent.mRanges = mClauses;
  }

  // While this method dispatches a composition event, some other event handler
  // cause more clauses to be added.  So, we should clear pending composition
  // before dispatching the event.
  Clear();
  rv = widget->DispatchEvent(&compChangeEvent, aStatus);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

} // namespace widget
} // namespace mozilla
