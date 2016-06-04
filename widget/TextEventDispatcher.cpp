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
  , mInputTransactionType(eNoInputTransaction)
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
  return BeginInputTransactionInternal(aListener,
                                       eSameProcessSyncInputTransaction);
}

nsresult
TextEventDispatcher::BeginTestInputTransaction(
                       TextEventDispatcherListener* aListener,
                       bool aIsAPZAware)
{
  return BeginInputTransactionInternal(aListener,
           aIsAPZAware ? eAsyncTestInputTransaction :
                         eSameProcessSyncTestInputTransaction);
}

nsresult
TextEventDispatcher::BeginNativeInputTransaction()
{
  if (NS_WARN_IF(!mWidget)) {
    return NS_ERROR_FAILURE;
  }
  RefPtr<TextEventDispatcherListener> listener =
    mWidget->GetNativeTextEventDispatcherListener();
  if (NS_WARN_IF(!listener)) {
    return NS_ERROR_FAILURE;
  }
  return BeginInputTransactionInternal(listener, eNativeInputTransaction);
}

nsresult
TextEventDispatcher::BeginInputTransactionInternal(
                       TextEventDispatcherListener* aListener,
                       InputTransactionType aType)
{
  if (NS_WARN_IF(!aListener)) {
    return NS_ERROR_INVALID_ARG;
  }
  nsCOMPtr<TextEventDispatcherListener> listener = do_QueryReferent(mListener);
  if (listener) {
    if (listener == aListener && mInputTransactionType == aType) {
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
  mInputTransactionType = aType;
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

  mInputTransactionType = eNoInputTransaction;

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
  mInputTransactionType = eNoInputTransaction;
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
  aEvent.mTime = PR_IntervalNow();
  aEvent.mRefPoint = LayoutDeviceIntPoint(0, 0);
  aEvent.mFlags.mIsSynthesizedForTests = IsForTests();
  if (aEvent.mClass != eCompositionEventClass) {
    return;
  }
  void* pseudoIMEContext = GetPseudoIMEContext();
  if (pseudoIMEContext) {
    aEvent.AsCompositionEvent()->mNativeIMEContext.
      InitWithRawNativeIMEContext(pseudoIMEContext);
  }
#ifdef DEBUG
  else {
    MOZ_ASSERT(!XRE_IsContentProcess(),
      "Why did the content process start native event transaction?");
    MOZ_ASSERT(aEvent.AsCompositionEvent()->mNativeIMEContext.IsValid(),
      "Native IME context shouldn't be invalid");
  }
#endif // #ifdef DEBUG
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
                                        nsEventStatus& aStatus)
{
  RefPtr<TextEventDispatcher> kungFuDeathGrip(this);
  nsCOMPtr<nsIWidget> widget(aWidget);
  mDispatchingEvent++;

  // If the event is dispatched via nsIWidget::DispatchInputEvent(), it
  // sends the event to the parent process first since APZ needs to handle it
  // first.  However, some callers (e.g., keyboard apps on B2G and tests
  // expecting synchronous dispatch) don't want this to do that.
  nsresult rv = NS_OK;
  if (ShouldSendInputEventToAPZ()) {
    aStatus = widget->DispatchInputEvent(&aEvent);
  } else {
    rv = widget->DispatchEvent(&aEvent, aStatus);
  }

  mDispatchingEvent--;
  return rv;
}

nsresult
TextEventDispatcher::StartComposition(nsEventStatus& aStatus,
                                      const WidgetEventTime* aEventTime)
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
  if (aEventTime) {
    compositionStartEvent.AssignEventTime(*aEventTime);
  }
  rv = DispatchEvent(mWidget, compositionStartEvent, aStatus);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
TextEventDispatcher::StartCompositionAutomaticallyIfNecessary(
                       nsEventStatus& aStatus,
                       const WidgetEventTime* aEventTime)
{
  if (IsComposing()) {
    return NS_OK;
  }

  nsresult rv = StartComposition(aStatus, aEventTime);
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
                                       const nsAString* aCommitString,
                                       const WidgetEventTime* aEventTime)
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
  rv = StartCompositionAutomaticallyIfNecessary(aStatus, aEventTime);
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
  if (aEventTime) {
    compositionCommitEvent.AssignEventTime(*aEventTime);
  }
  if (message == eCompositionCommit) {
    compositionCommitEvent.mData = *aCommitString;
    // Don't send CRLF, replace it with LF here.
    compositionCommitEvent.mData.ReplaceSubstring(NS_LITERAL_STRING("\r\n"),
                                                  NS_LITERAL_STRING("\n"));
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
  nsresult rv = NS_ERROR_NOT_IMPLEMENTED;

  // First, send the notification to current input transaction's listener.
  nsCOMPtr<TextEventDispatcherListener> listener = do_QueryReferent(mListener);
  if (listener) {
    rv = listener->NotifyIME(this, aIMENotification);
  }

  if (mInputTransactionType == eNativeInputTransaction || !mWidget) {
    return rv;
  }

  // If current input transaction isn't for native event handler, we should
  // send the notification to the native text event dispatcher listener
  // since native event handler may need to do something from
  // TextEventDispatcherListener::NotifyIME() even before there is no
  // input transaction yet.  For example, native IME handler may need to
  // create new context at receiving NOTIFY_IME_OF_FOCUS.  In this case,
  // mListener may not be initialized since input transaction should be
  // initialized immediately before dispatching every WidgetKeyboardEvent
  // and WidgetCompositionEvent (dispatching events always occurs after
  // focus move).
  nsCOMPtr<TextEventDispatcherListener> nativeListener =
    mWidget->GetNativeTextEventDispatcherListener();
  if (!nativeListener) {
    return rv;
  }
  switch (aIMENotification.mMessage) {
    case REQUEST_TO_COMMIT_COMPOSITION:
    case REQUEST_TO_CANCEL_COMPOSITION:
      // It's not necessary to notify native IME of requests.
      return rv;
    default: {
      // Even if current input transaction's listener returns NS_OK or
      // something, we need to notify native IME of notifications because
      // when user typing after TIP does something, the changed information
      // is necessary for them.
      nsresult rv2 =
        nativeListener->NotifyIME(this, aIMENotification);
      // But return the result from current listener except when the
      // notification isn't handled.
      return rv == NS_ERROR_NOT_IMPLEMENTED ? rv2 : rv;
    }
  }
}

bool
TextEventDispatcher::DispatchKeyboardEvent(
                       EventMessage aMessage,
                       const WidgetKeyboardEvent& aKeyboardEvent,
                       nsEventStatus& aStatus,
                       void* aData)
{
  return DispatchKeyboardEventInternal(aMessage, aKeyboardEvent, aStatus,
                                       aData);
}

bool
TextEventDispatcher::DispatchKeyboardEventInternal(
                       EventMessage aMessage,
                       const WidgetKeyboardEvent& aKeyboardEvent,
                       nsEventStatus& aStatus,
                       void* aData,
                       uint32_t aIndexOfKeypress)
{
  // Note that this method is also used for dispatching key events on a plugin
  // because key events on a plugin should be dispatched same as normal key
  // events.  Then, only some handlers which need to intercept key events
  // before the focused plugin (e.g., reserved shortcut key handlers) can
  // consume the events.
  MOZ_ASSERT(WidgetKeyboardEvent::IsKeyDownOrKeyDownOnPlugin(aMessage) ||
             WidgetKeyboardEvent::IsKeyUpOrKeyUpOnPlugin(aMessage) ||
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
  // Note that plugin process has different IME context.  Therefore, we don't
  // need to check our composition state when the key event is fired on a
  // plugin.
  if (IsComposing() && !WidgetKeyboardEvent::IsKeyEventOnPlugin(aMessage)) {
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
    keyEvent.PreventDefaultBeforeDispatch();
  }

  // Corrects each member for the specific key event type.
  if (keyEvent.mKeyNameIndex != KEY_NAME_INDEX_USE_STRING) {
    MOZ_ASSERT(!aIndexOfKeypress,
      "aIndexOfKeypress must be 0 for non-printable key");
    // If the keyboard event isn't caused by printable key, its charCode should
    // be 0.
    keyEvent.SetCharCode(0);
  } else {
    if (WidgetKeyboardEvent::IsKeyDownOrKeyDownOnPlugin(aMessage) ||
        WidgetKeyboardEvent::IsKeyUpOrKeyUpOnPlugin(aMessage)) {
      MOZ_RELEASE_ASSERT(!aIndexOfKeypress,
        "aIndexOfKeypress must be 0 for either eKeyDown or eKeyUp");
    } else {
      MOZ_RELEASE_ASSERT(
        !aIndexOfKeypress || aIndexOfKeypress < keyEvent.mKeyValue.Length(),
        "aIndexOfKeypress must be 0 - mKeyValue.Length() - 1");
    }
    wchar_t ch =
      keyEvent.mKeyValue.IsEmpty() ? 0 : keyEvent.mKeyValue[aIndexOfKeypress];
    keyEvent.SetCharCode(static_cast<uint32_t>(ch));
    if (aMessage == eKeyPress) {
      // keyCode of eKeyPress events of printable keys should be always 0.
      keyEvent.mKeyCode = 0;
      // eKeyPress events are dispatched for every character.
      // So, each key value of eKeyPress events should be a character.
      if (ch) {
        keyEvent.mKeyValue.Assign(ch);
      } else {
        keyEvent.mKeyValue.Truncate();
      }
    }
  }
  if (WidgetKeyboardEvent::IsKeyUpOrKeyUpOnPlugin(aMessage)) {
    // mIsRepeat of keyup event must be false.
    keyEvent.mIsRepeat = false;
  }
  // mIsComposing should be initialized later.
  keyEvent.mIsComposing = false;
  if (mInputTransactionType == eNativeInputTransaction) {
    // Copy mNativeKeyEvent here because for safety for other users of
    // AssignKeyEventData(), it doesn't copy this.
    keyEvent.mNativeKeyEvent = aKeyboardEvent.mNativeKeyEvent;
  } else {
    // If it's not a keyboard event for native key event, we should ensure that
    // mNativeKeyEvent and mPluginEvent are null/empty.
    keyEvent.mNativeKeyEvent = nullptr;
    keyEvent.mPluginEvent.Clear();
  }
  // TODO: Manage mUniqueId here.

  // Request the alternative char codes for the key event.
  // eKeyDown also needs alternative char codes because nsXBLWindowKeyHandler
  // needs to check if a following keypress event is reserved by chrome for
  // stopping propagation of its preceding keydown event.
  keyEvent.mAlternativeCharCodes.Clear();
  if ((WidgetKeyboardEvent::IsKeyDownOrKeyDownOnPlugin(aMessage) ||
       aMessage == eKeyPress) &&
      (keyEvent.IsControl() || keyEvent.IsAlt() ||
       keyEvent.IsMeta() || keyEvent.IsOS())) {
    nsCOMPtr<TextEventDispatcherListener> listener =
      do_QueryReferent(mListener);
    if (listener) {
      DebugOnly<WidgetKeyboardEvent> original(keyEvent);
      listener->WillDispatchKeyboardEvent(this, keyEvent, aIndexOfKeypress,
                                          aData);
      MOZ_ASSERT(keyEvent.mMessage ==
                   static_cast<WidgetKeyboardEvent&>(original).mMessage);
      MOZ_ASSERT(keyEvent.mKeyCode ==
                   static_cast<WidgetKeyboardEvent&>(original).mKeyCode);
      MOZ_ASSERT(keyEvent.mLocation ==
                   static_cast<WidgetKeyboardEvent&>(original).mLocation);
      MOZ_ASSERT(keyEvent.mIsRepeat ==
                   static_cast<WidgetKeyboardEvent&>(original).mIsRepeat);
      MOZ_ASSERT(keyEvent.mIsComposing ==
                   static_cast<WidgetKeyboardEvent&>(original).mIsComposing);
      MOZ_ASSERT(keyEvent.mKeyNameIndex ==
                   static_cast<WidgetKeyboardEvent&>(original).mKeyNameIndex);
      MOZ_ASSERT(keyEvent.mCodeNameIndex ==
                   static_cast<WidgetKeyboardEvent&>(original).mCodeNameIndex);
      MOZ_ASSERT(keyEvent.mKeyValue ==
                   static_cast<WidgetKeyboardEvent&>(original).mKeyValue);
      MOZ_ASSERT(keyEvent.mCodeValue ==
                   static_cast<WidgetKeyboardEvent&>(original).mCodeValue);
    }
  }

  DispatchInputEvent(mWidget, keyEvent, aStatus);
  return true;
}

bool
TextEventDispatcher::MaybeDispatchKeypressEvents(
                       const WidgetKeyboardEvent& aKeyboardEvent,
                       nsEventStatus& aStatus,
                       void* aData)
{
  // If the key event was consumed, keypress event shouldn't be fired.
  if (aStatus == nsEventStatus_eConsumeNoDefault) {
    return false;
  }

  // If the key shouldn't cause keypress events, don't fire them.
  if (!aKeyboardEvent.ShouldCauseKeypressEvents()) {
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
                                       aStatus, aData, i)) {
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
  mCaret.mRangeType = TextRangeType::NS_TEXTRANGE_UNDEFINED;
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
TextEventDispatcher::PendingComposition::AppendClause(
                                           uint32_t aLength,
                                           TextRangeType aTextRangeType)
{
  if (NS_WARN_IF(!aLength)) {
    return NS_ERROR_INVALID_ARG;
  }

  switch (aTextRangeType) {
    case TextRangeType::NS_TEXTRANGE_RAWINPUT:
    case TextRangeType::NS_TEXTRANGE_SELECTEDRAWTEXT:
    case TextRangeType::NS_TEXTRANGE_CONVERTEDTEXT:
    case TextRangeType::NS_TEXTRANGE_SELECTEDCONVERTEDTEXT: {
      EnsureClauseArray();
      TextRange textRange;
      textRange.mStartOffset =
        mClauses->IsEmpty() ? 0 : mClauses->LastElement().mEndOffset;
      textRange.mEndOffset = textRange.mStartOffset + aLength;
      textRange.mRangeType = aTextRangeType;
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
  mCaret.mRangeType = TextRangeType::NS_TEXTRANGE_CARETPOSITION;
  return NS_OK;
}

nsresult
TextEventDispatcher::PendingComposition::Set(const nsAString& aString,
                                             const TextRangeArray* aRanges)
{
  Clear();

  nsAutoString str(aString);
  // Don't expose CRLF to web contents, instead, use LF.
  str.ReplaceSubstring(NS_LITERAL_STRING("\r\n"), NS_LITERAL_STRING("\n"));
  nsresult rv = SetString(str);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!aRanges || aRanges->IsEmpty()) {
    // Create dummy range if aString isn't empty.
    if (!aString.IsEmpty()) {
      rv = AppendClause(str.Length(), TextRangeType::NS_TEXTRANGE_RAWINPUT);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
    return NS_OK;
  }

  // Adjust offsets in the ranges for XP linefeed character (only \n).
  // XXX Following code is the safest approach.  However, it wastes performance.
  //     For ensuring the clauses do not overlap each other, we should redesign
  //     TextRange later.
  for (uint32_t i = 0; i < aRanges->Length(); ++i) {
    TextRange range = aRanges->ElementAt(i);
    TextRange nativeRange = range;
    if (nativeRange.mStartOffset > 0) {
      nsAutoString preText(Substring(aString, 0, nativeRange.mStartOffset));
      preText.ReplaceSubstring(NS_LITERAL_STRING("\r\n"),
                               NS_LITERAL_STRING("\n"));
      range.mStartOffset = preText.Length();
    }
    if (nativeRange.Length() == 0) {
      range.mEndOffset = range.mStartOffset;
    } else {
      nsAutoString clause(
        Substring(aString, nativeRange.mStartOffset, nativeRange.Length()));
      clause.ReplaceSubstring(NS_LITERAL_STRING("\r\n"),
                              NS_LITERAL_STRING("\n"));
      range.mEndOffset = range.mStartOffset + clause.Length();
    }
    if (range.mRangeType == TextRangeType::NS_TEXTRANGE_CARETPOSITION) {
      mCaret = range;
    } else {
      EnsureClauseArray();
      mClauses->AppendElement(range);
    }
  }
  return NS_OK;
}

nsresult
TextEventDispatcher::PendingComposition::Flush(
                                           TextEventDispatcher* aDispatcher,
                                           nsEventStatus& aStatus,
                                           const WidgetEventTime* aEventTime)
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
  if (mCaret.mRangeType == TextRangeType::NS_TEXTRANGE_CARETPOSITION) {
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
  if (aEventTime) {
    compChangeEvent.AssignEventTime(*aEventTime);
  }
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

  rv = aDispatcher->StartCompositionAutomaticallyIfNecessary(aStatus,
                                                             aEventTime);
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
