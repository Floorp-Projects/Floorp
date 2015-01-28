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

  if (NS_WARN_IF(!aDispatcher->mInitialized)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsCOMPtr<nsIWidget> widget(aDispatcher->mWidget);
  if (NS_WARN_IF(!widget || widget->Destroyed())) {
    return NS_ERROR_NOT_AVAILABLE;
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

  WidgetCompositionEvent compChangeEvent(true, NS_COMPOSITION_CHANGE, widget);
  compChangeEvent.time = PR_IntervalNow();
  compChangeEvent.mData = mString;
  if (mClauses) {
    MOZ_ASSERT(!mClauses->IsEmpty(),
               "mClauses must be non-empty array when it's not nullptr");
    compChangeEvent.mRanges = mClauses;
  }

  compChangeEvent.mFlags.mIsSynthesizedForTests = aDispatcher->mForTests;

  // While this method dispatches a composition event, some other event handler
  // cause more clauses to be added.  So, we should clear pending composition
  // before dispatching the event.
  Clear();
  nsresult rv = widget->DispatchEvent(&compChangeEvent, aStatus);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

} // namespace widget
} // namespace mozilla
