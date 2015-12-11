/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Preferences.h"
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

bool TextEventDispatcher::sDispatchKeyEventsDuringComposition = false;

TextEventDispatcher::TextEventDispatcher(nsIWidget* aWidget)
  : mWidget(aWidget)
  , mDispatchingEvent(0)
  , mForTests(false)
  , mIsComposing(false)
{
  MOZ_RELEASE_ASSERT(mWidget, "aWidget must not be nullptr");

  static bool sInitialized = false;
  if (!sInitialized) {
    Preferences::AddBoolVarCache(
      &sDispatchKeyEventsDuringComposition,
      "dom.keyboardevent.dispatch_during_composition",
      false);
    sInitialized = true;
  }
}

nsresult
TextEventDispatcher::BeginInputTransaction(
                       TextEventDispatcherListener* aListener)
{
  return BeginInputTransactionInternal(aListener, false);
}

nsresult
TextEventDispatcher::BeginInputTransactionForTests(
                       TextEventDispatcherListener* aListener)
{
  return BeginInputTransactionInternal(aListener, true);
}

nsresult
TextEventDispatcher::BeginInputTransactionInternal(
                       TextEventDispatcherListener* aListener,
                       bool aForTests)
{
  if (NS_WARN_IF(!aListener)) {
    return NS_ERROR_INVALID_ARG;
  }
  nsCOMPtr<TextEventDispatcherListener> listener = do_QueryReferent(mListener);
  if (listener) {
    if (listener == aListener && mForTests == aForTests) {
      return NS_OK;
    }
    // If this has composition or is dispatching an event, any other listener
    // can steal ownership.  Especially, if the latter case is allowed,
    // nobody cannot begin input transaction with this if a modal dialog is
    // opened during dispatching an event.
    if (IsComposing() || IsDispatchingEvent()) {
      return NS_ERROR_ALREADY_INITIALIZED;
    }
  }
  mListener = do_GetWeakReference(aListener);
  mForTests = aForTests;
  if (listener && listener != aListener) {
    listener->OnRemovedFrom(this);
  }
  return NS_OK;
}

void
TextEventDispatcher::EndInputTransaction(TextEventDispatcherListener* aListener)
{
  if (NS_WARN_IF(IsComposing()) || NS_WARN_IF(IsDispatchingEvent())) {
    return;
  }

  nsCOMPtr<TextEventDispatcherListener> listener = do_QueryReferent(mListener);
  if (NS_WARN_IF(!listener)) {
    return;
  }

  if (NS_WARN_IF(listener != aListener)) {
    return;
  }

  mListener = nullptr;
  listener->OnRemovedFrom(this);
}

void
TextEventDispatcher::OnDestroyWidget()
{
  mWidget = nullptr;
  mPendingComposition.Clear();
  nsCOMPtr<TextEventDispatcherListener> listener = do_QueryReferent(mListener);
  mListener = nullptr;
  if (listener) {
    listener->OnRemovedFrom(this);
  }
}

nsresult
TextEventDispatcher::GetState() const
{
  nsCOMPtr<TextEventDispatcherListener> listener = do_QueryReferent(mListener);
  if (!listener) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  if (!mWidget || mWidget->Destroyed()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return NS_OK;
}

void
TextEventDispatcher::InitEvent(WidgetGUIEvent& aEvent) const
{
  aEvent.time = PR_IntervalNow();
  aEvent.refPoint = LayoutDeviceIntPoint(0, 0);
  aEvent.mFlags.mIsSynthesizedForTests = mForTests;
  if (aEvent.mClass != eCompositionEventClass) {
    return;
  }
  // Currently, we should set special native IME context when composition
  // events are dispatched from PuppetWidget since PuppetWidget may have not
  // known actual native IME context yet and it caches native IME context
  // when it dispatches every WidgetCompositionEvent.
  if (XRE_IsContentProcess()) {
    aEvent.AsCompositionEvent()->
      mNativeIMEContext.InitWithRawNativeIMEContext(mWidget);
  }
}

nsresult
TextEventDispatcher::DispatchEvent(nsIWidget* aWidget,
                                   WidgetGUIEvent& aEvent,
                                   nsEventStatus& aStatus)
{
  MOZ_ASSERT(!aEvent.AsInputEvent(), "Use DispatchInputEvent()");

  RefPtr<TextEventDispatcher> kungFuDeathGrip(this);
  nsCOMPtr<nsIWidget> widget(aWidget);
  mDispatchingEvent++;
  nsresult rv = widget->DispatchEvent(&aEvent, aStatus);
  mDispatchingEvent--;
  return rv;
}

nsresult
TextEventDispatcher::DispatchInputEvent(nsIWidget* aWidget,
                                        WidgetInputEvent& aEvent,
                                        nsEventStatus& aStatus,
                                        DispatchTo aDispatchTo)
{
  RefPtr<TextEventDispatcher> kungFuDeathGrip(this);
  nsCOMPtr<nsIWidget> widget(aWidget);
  mDispatchingEvent++;

  // If the event is dispatched via nsIWidget::DispatchInputEvent(), it
  // sends the event to the parent process first since APZ needs to handle it
  // first.  However, some callers (e.g., keyboard apps on B2G and tests
  // expecting synchronous dispatch) don't want this to do that.
  nsresult rv = NS_OK;
  if (aDispatchTo == eDispatchToParentProcess) {
    aStatus = widget->DispatchInputEvent(&aEvent);
  } else {
    rv = widget->DispatchEvent(&aEvent, aStatus);
  }

  mDispatchingEvent--;
  return rv;
}

nsresult
TextEventDispatcher::StartComposition(nsEventStatus& aStatus)
{
  aStatus = nsEventStatus_eIgnore;

  nsresult rv = GetState();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(mIsComposing)) {
    return NS_ERROR_FAILURE;
  }

  mIsComposing = true;
  WidgetCompositionEvent compositionStartEvent(true, eCompositionStart,
                                               mWidget);
  InitEvent(compositionStartEvent);
  rv = DispatchEvent(mWidget, compositionStartEvent, aStatus);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
TextEventDispatcher::StartCompositionAutomaticallyIfNecessary(
                       nsEventStatus& aStatus)
{
  if (IsComposing()) {
    return NS_OK;
  }

  nsresult rv = StartComposition(aStatus);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // If started composition has already been committed, we shouldn't dispatch
  // the compositionchange event.
  if (!IsComposing()) {
    aStatus = nsEventStatus_eConsumeNoDefault;
    return NS_OK;
  }

  // Note that the widget might be destroyed during a call of
  // StartComposition().  In such case, we shouldn't keep dispatching next
  // event.
  rv = GetState();
  if (NS_FAILED(rv)) {
    MOZ_ASSERT(rv != NS_ERROR_NOT_INITIALIZED,
               "aDispatcher must still be initialized in this case");
    aStatus = nsEventStatus_eConsumeNoDefault;
    return NS_OK; // Don't throw exception in this case
  }

  aStatus = nsEventStatus_eIgnore;
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

  // When there is no composition, caller shouldn't try to commit composition
  // with non-existing composition string nor commit composition with empty
  // string.
  if (NS_WARN_IF(!IsComposing() &&
                 (!aCommitString || aCommitString->IsEmpty()))) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIWidget> widget(mWidget);
  rv = StartCompositionAutomaticallyIfNecessary(aStatus);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (aStatus == nsEventStatus_eConsumeNoDefault) {
    return NS_OK;
  }

  // End current composition and make this free for other IMEs.
  mIsComposing = false;

  EventMessage message = aCommitString ? eCompositionCommit :
                                         eCompositionCommitAsIs;
  WidgetCompositionEvent compositionCommitEvent(true, message, widget);
  InitEvent(compositionCommitEvent);
  if (message == eCompositionCommit) {
    compositionCommitEvent.mData = *aCommitString;
  }
  rv = DispatchEvent(widget, compositionCommitEvent, aStatus);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
TextEventDispatcher::NotifyIME(const IMENotification& aIMENotification)
{
  nsCOMPtr<TextEventDispatcherListener> listener = do_QueryReferent(mListener);
  if (!listener) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  nsresult rv = listener->NotifyIME(this, aIMENotification);
  // If the listener isn't available, it means that it cannot handle the
  // notification or request for now.  In this case, we should return
  // NS_ERROR_NOT_IMPLEMENTED because it's not implemented at such moment.
  if (rv == NS_ERROR_NOT_AVAILABLE) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  return rv;
}

bool
TextEventDispatcher::DispatchKeyboardEvent(
                       EventMessage aMessage,
                       const WidgetKeyboardEvent& aKeyboardEvent,
                       nsEventStatus& aStatus,
                       DispatchTo aDispatchTo)
{
  return DispatchKeyboardEventInternal(aMessage, aKeyboardEvent, aStatus,
                                       aDispatchTo);
}

bool
TextEventDispatcher::DispatchKeyboardEventInternal(
                       EventMessage aMessage,
                       const WidgetKeyboardEvent& aKeyboardEvent,
                       nsEventStatus& aStatus,
                       DispatchTo aDispatchTo,
                       uint32_t aIndexOfKeypress)
{
  MOZ_ASSERT(aMessage == eKeyDown || aMessage == eKeyUp ||
             aMessage == eKeyPress, "Invalid aMessage value");
  nsresult rv = GetState();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  // If the key shouldn't cause keypress events, don't this patch them.
  if (aMessage == eKeyPress && !aKeyboardEvent.ShouldCauseKeypressEvents()) {
    return false;
  }

  // Basically, key events shouldn't be dispatched during composition.
  if (IsComposing()) {
    // However, if we need to behave like other browsers, we need the keydown
    // and keyup events.  Note that this behavior is also allowed by D3E spec.
    // FYI: keypress events must not be fired during composition.
    if (!sDispatchKeyEventsDuringComposition || aMessage == eKeyPress) {
      return false;
    }
    // XXX If there was mOnlyContentDispatch for this case, it might be useful
    //     because our chrome doesn't assume that key events are fired during
    //     composition.
  }

  WidgetKeyboardEvent keyEvent(true, aMessage, mWidget);
  InitEvent(keyEvent);
  keyEvent.AssignKeyEventData(aKeyboardEvent, false);

  if (aStatus == nsEventStatus_eConsumeNoDefault) {
    // If the key event should be dispatched as consumed event, marking it here.
    // This is useful to prevent double action.  E.g., when the key was already
    // handled by system, our chrome shouldn't handle it.
    keyEvent.mFlags.mDefaultPrevented = true;
  }

  // Corrects each member for the specific key event type.
  if (aMessage == eKeyDown || aMessage == eKeyUp) {
    MOZ_ASSERT(!aIndexOfKeypress,
      "aIndexOfKeypress must be 0 for either eKeyDown or eKeyUp");
    // charCode of keydown and keyup should be 0.
    keyEvent.charCode = 0;
  } else if (keyEvent.mKeyNameIndex != KEY_NAME_INDEX_USE_STRING) {
    MOZ_ASSERT(!aIndexOfKeypress,
      "aIndexOfKeypress must be 0 for eKeyPress of non-printable key");
    // If keypress event isn't caused by printable key, its charCode should
    // be 0.
    keyEvent.charCode = 0;
  } else {
    MOZ_RELEASE_ASSERT(
      !aIndexOfKeypress || aIndexOfKeypress < keyEvent.mKeyValue.Length(),
      "aIndexOfKeypress must be 0 - mKeyValue.Length() - 1");
    keyEvent.keyCode = 0;
    wchar_t ch =
      keyEvent.mKeyValue.IsEmpty() ? 0 : keyEvent.mKeyValue[aIndexOfKeypress];
    keyEvent.charCode = static_cast<uint32_t>(ch);
    if (ch) {
      keyEvent.mKeyValue.Assign(ch);
    } else {
      keyEvent.mKeyValue.Truncate();
    }
  }
  if (aMessage == eKeyUp) {
    // mIsRepeat of keyup event must be false.
    keyEvent.mIsRepeat = false;
  }
  // mIsComposing should be initialized later.
  keyEvent.mIsComposing = false;
  // XXX Currently, we don't support to dispatch key event with native key
  //     event information.
  keyEvent.mNativeKeyEvent = nullptr;
  // XXX Currently, we don't support to dispatch key events with data for
  // plugins.
  keyEvent.mPluginEvent.Clear();
  // TODO: Manage mUniqueId here.

  DispatchInputEvent(mWidget, keyEvent, aStatus, aDispatchTo);
  return true;
}

bool
TextEventDispatcher::MaybeDispatchKeypressEvents(
                       const WidgetKeyboardEvent& aKeyboardEvent,
                       nsEventStatus& aStatus,
                       DispatchTo aDispatchTo)
{
  // If the key event was consumed, keypress event shouldn't be fired.
  if (aStatus == nsEventStatus_eConsumeNoDefault) {
    return false;
  }

  // If the key isn't a printable key or just inputting one character or
  // no character, we should dispatch only one keypress.  Otherwise, i.e.,
  // if the key is a printable key and inputs multiple characters, keypress
  // event should be dispatched the count of inputting characters times.
  size_t keypressCount =
    aKeyboardEvent.mKeyNameIndex != KEY_NAME_INDEX_USE_STRING ?
      1 : std::max(static_cast<nsAString::size_type>(1),
                   aKeyboardEvent.mKeyValue.Length());
  bool isDispatched = false;
  bool consumed = false;
  for (size_t i = 0; i < keypressCount; i++) {
    aStatus = nsEventStatus_eIgnore;
    if (!DispatchKeyboardEventInternal(eKeyPress, aKeyboardEvent,
                                       aStatus, aDispatchTo, i)) {
      // The widget must have been gone.
      break;
    }
    isDispatched = true;
    if (!consumed) {
      consumed = (aStatus == nsEventStatus_eConsumeNoDefault);
    }
  }

  // If one of the keypress event was consumed, return ConsumeNoDefault.
  if (consumed) {
    aStatus = nsEventStatus_eConsumeNoDefault;
  }

  return isDispatched;
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
TextEventDispatcher::PendingComposition::Flush(TextEventDispatcher* aDispatcher,
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

  RefPtr<TextEventDispatcher> kungFuDeathGrip(aDispatcher);
  nsCOMPtr<nsIWidget> widget(aDispatcher->mWidget);
  WidgetCompositionEvent compChangeEvent(true, eCompositionChange, widget);
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

  rv = aDispatcher->StartCompositionAutomaticallyIfNecessary(aStatus);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (aStatus == nsEventStatus_eConsumeNoDefault) {
    return NS_OK;
  }
  rv = aDispatcher->DispatchEvent(widget, compChangeEvent, aStatus);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

} // namespace widget
} // namespace mozilla
