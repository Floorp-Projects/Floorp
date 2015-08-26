/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_textcompositionsynthesizer_h_
#define mozilla_textcompositionsynthesizer_h_

#include "nsAutoPtr.h"
#include "nsString.h"
#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "mozilla/TextEventDispatcherListener.h"
#include "mozilla/TextRange.h"

class nsIWidget;

namespace mozilla {
namespace widget {

struct IMENotification;

/**
 * TextEventDispatcher is a helper class for dispatching widget events defined
 * in TextEvents.h.  Currently, this is a helper for dispatching
 * WidgetCompositionEvent and WidgetKeyboardEvent.  This manages the behavior
 * of them for conforming to DOM Level 3 Events.
 * An instance of this class is created by nsIWidget instance and owned by it.
 * This is typically created only by the top level widgets because only they
 * handle IME.
 */

class TextEventDispatcher final
{
  ~TextEventDispatcher()
  {
  }

  NS_INLINE_DECL_REFCOUNTING(TextEventDispatcher)

public:
  explicit TextEventDispatcher(nsIWidget* aWidget);

  /**
   * Initializes the instance for IME or automated test.  Either IME or tests
   * need to call one of them before starting composition.  If they return
   * NS_ERROR_ALREADY_INITIALIZED, it means that the listener already listens
   * notifications from TextEventDispatcher for same purpose (for IME or tests).
   * If this returns another error, the caller shouldn't keep starting
   * composition.
   *
   * @param aListener       Specify the listener to listen notifications and
   *                        requests.  This must not be null.
   *                        NOTE: aListener is stored as weak reference in
   *                              TextEventDispatcher.  See mListener
   *                              definition below.
   */
  nsresult BeginInputTransaction(TextEventDispatcherListener* aListener);
  nsresult BeginInputTransactionForTests(
             TextEventDispatcherListener* aListener);

  /**
   * EndInputTransaction() should be called when the listener stops using
   * the TextEventDispatcher.
   *
   * @param aListener       The listener using the TextEventDispatcher instance.
   */
  void EndInputTransaction(TextEventDispatcherListener* aListener);

  /**
   * OnDestroyWidget() is called when mWidget is being destroyed.
   */
  void OnDestroyWidget();

  /**
   * GetState() returns current state of this class.
   *
   * @return        NS_OK: Fine to compose text.
   *                NS_ERROR_NOT_INITIALIZED: BeginInputTransaction() or
   *                                          BeginInputTransactionForTests()
   *                                          should be called.
   *                NS_ERROR_NOT_AVAILABLE: The widget isn't available for
   *                                        composition.
   */
  nsresult GetState() const;

  /**
   * IsComposing() returns true after calling StartComposition() and before
   * calling CommitComposition().
   */
  bool IsComposing() const { return mIsComposing; }

  /**
   * IsDispatchingEvent() returns true while this instance dispatching an event.
   */
  bool IsDispatchingEvent() const { return mDispatchingEvent > 0; }

  /**
   * StartComposition() starts composition explicitly.
   */
  nsresult StartComposition(nsEventStatus& aStatus);

  /**
   * CommitComposition() commits composition.
   *
   * @param aCommitString   If this is null, commits with the last composition
   *                        string.  Otherwise, commits the composition with
   *                        this value.
   */
   nsresult CommitComposition(nsEventStatus& aStatus,
                              const nsAString* aCommitString = nullptr);

  /**
   * SetPendingCompositionString() sets new composition string which will be
   * dispatched with NS_COMPOSITION_CHANGE event by calling Flush().
   *
   * @param aString         New composition string.
   */
  nsresult SetPendingCompositionString(const nsAString& aString)
  {
    return mPendingComposition.SetString(aString);
  }

  /**
   * AppendClauseToPendingComposition() appends a clause information to
   * the pending composition string.
   *
   * @param aLength         Length of the clause.
   * @param aAttribute      One of NS_TEXTRANGE_RAWINPUT,
   *                        NS_TEXTRANGE_SELECTEDRAWTEXT,
   *                        NS_TEXTRANGE_CONVERTEDTEXT or
   *                        NS_TEXTRANGE_SELECTEDCONVERTEDTEXT.
   */
  nsresult AppendClauseToPendingComposition(uint32_t aLength,
                                            uint32_t aAttribute)
  {
    return mPendingComposition.AppendClause(aLength, aAttribute);
  }

  /**
   * SetCaretInPendingComposition() sets caret position in the pending
   * composition string and its length.  This is optional.  If IME doesn't
   * want to show caret, it shouldn't need to call this.
   *
   * @param aOffset         Offset of the caret in the pending composition
   *                        string.  This should not be larger than the length
   *                        of the pending composition string.
   * @param aLength         Caret width.  If this is 0, caret will be collapsed.
   *                        Note that Gecko doesn't supported wide caret yet,
   *                        therefore, this is ignored for now.
   */
  nsresult SetCaretInPendingComposition(uint32_t aOffset,
                                        uint32_t aLength)
  {
    return mPendingComposition.SetCaret(aOffset, aLength);
  }

  /**
   * FlushPendingComposition() sends the pending composition string
   * to the widget of the store DOM window.  Before calling this, IME needs to
   * set pending composition string with SetPendingCompositionString(),
   * AppendClauseToPendingComposition() and/or
   * SetCaretInPendingComposition().
   */
  nsresult FlushPendingComposition(nsEventStatus& aStatus)
  {
    return mPendingComposition.Flush(this, aStatus);
  }

  /**
   * ClearPendingComposition() makes this instance forget pending composition.
   */
  void ClearPendingComposition()
  {
    mPendingComposition.Clear();
  }

  /**
   * @see nsIWidget::NotifyIME()
   */
  nsresult NotifyIME(const IMENotification& aIMENotification);


  /**
   * DispatchTo indicates whether the event may be dispatched to its parent
   * process first (if there is) or not.  If the event is dispatched to the
   * parent process, APZ will handle it first.  Otherwise, the event won't be
   * handled by APZ if it's in a child process.
   */
  enum DispatchTo
  {
    // The event may be dispatched to its parent process if there is a parent.
    // In such case, the event will be handled asynchronously.  Additionally,
    // the event may be sent to its child process if a child process (including
    // the dispatching process) has focus.
    eDispatchToParentProcess = 0,
    // The event must be dispatched in the current process.  But of course,
    // the event may be sent to a child process when it has focus.  If there is
    // no child process, the event may be handled synchronously.
    eDispatchToCurrentProcess = 1
  };

  /**
   * DispatchKeyboardEvent() maybe dispatches aKeyboardEvent.
   *
   * @param aMessage        Must be NS_KEY_DOWN or NS_KEY_UP.
   *                        Use MaybeDispatchKeypressEvents() for dispatching
   *                        NS_KEY_PRESS.
   * @param aKeyboardEvent  A keyboard event.
   * @param aStatus         If dispatching event should be marked as consumed,
   *                        set nsEventStatus_eConsumeNoDefault.  Otherwise,
   *                        set nsEventStatus_eIgnore.  After dispatching
   *                        a event and it's consumed this returns
   *                        nsEventStatus_eConsumeNoDefault.
   * @param aDispatchTo     See comments of DispatchTo.
   * @return                true if an event is dispatched.  Otherwise, false.
   */
  bool DispatchKeyboardEvent(EventMessage aMessage,
                             const WidgetKeyboardEvent& aKeyboardEvent,
                             nsEventStatus& aStatus,
                             DispatchTo aDispatchTo = eDispatchToParentProcess);

  /**
   * MaybeDispatchKeypressEvents() maybe dispatches a keypress event which is
   * generated from aKeydownEvent.
   *
   * @param aKeyboardEvent  A keyboard event.
   * @param aStatus         Sets the result when the caller dispatches
   *                        aKeyboardEvent.  Note that if the value is
   *                        nsEventStatus_eConsumeNoDefault, this does NOT
   *                        dispatch keypress events.
   *                        When this method dispatches one or more keypress
   *                        events and one of them is consumed, this returns
   *                        nsEventStatus_eConsumeNoDefault.
   * @param aDispatchTo     See comments of DispatchTo.
   * @return                true if one or more events are dispatched.
   *                        Otherwise, false.
   */
  bool MaybeDispatchKeypressEvents(const WidgetKeyboardEvent& aKeyboardEvent,
                                   nsEventStatus& aStatus,
                                   DispatchTo aDispatchTo =
                                     eDispatchToParentProcess);

private:
  // mWidget is owner of the instance.  When this is created, this is set.
  // And when mWidget is released, this is cleared by OnDestroyWidget().
  // Note that mWidget may be destroyed already (i.e., mWidget->Destroyed() may
  // return true).
  nsIWidget* mWidget;
  // mListener is a weak reference to TextEventDispatcherListener.  That might
  // be referred by JS.  Therefore, the listener might be difficult to release
  // itself if this is a strong reference.  Additionally, it's difficult to
  // check if a method to uninstall the listener is called by valid instance.
  // So, using weak reference is the best way in this case.
  nsWeakPtr mListener;

  // mPendingComposition stores new composition string temporarily.
  // These values will be used for dispatching NS_COMPOSITION_CHANGE event
  // in Flush().  When Flush() is called, the members will be cleared
  // automatically.
  class PendingComposition
  {
  public:
    PendingComposition();
    nsresult SetString(const nsAString& aString);
    nsresult AppendClause(uint32_t aLength, uint32_t aAttribute);
    nsresult SetCaret(uint32_t aOffset, uint32_t aLength);
    nsresult Flush(TextEventDispatcher* aDispatcher, nsEventStatus& aStatus);
    void Clear();

  private:
    nsAutoString mString;
    nsRefPtr<TextRangeArray> mClauses;
    TextRange mCaret;

    void EnsureClauseArray();
  };
  PendingComposition mPendingComposition;

  // While dispatching an event, this is incremented.
  uint16_t mDispatchingEvent;

  bool mForTests;
  // See IsComposing().
  bool mIsComposing;

  // If this is true, keydown and keyup events are dispatched even when there
  // is a composition.
  static bool sDispatchKeyEventsDuringComposition;

  nsresult BeginInputTransactionInternal(
             TextEventDispatcherListener* aListener,
             bool aForTests);

  /**
   * InitEvent() initializes aEvent.  This must be called before dispatching
   * the event.
   */
  void InitEvent(WidgetGUIEvent& aEvent) const;


  /**
   * DispatchEvent() dispatches aEvent on aWidget.
   */
  nsresult DispatchEvent(nsIWidget* aWidget,
                         WidgetGUIEvent& aEvent,
                         nsEventStatus& aStatus);

  /**
   * DispatchInputEvent() dispatches aEvent on aWidget.
   *
   * @param aDispatchTo     See comments of DispatchTo.
   */
  nsresult DispatchInputEvent(nsIWidget* aWidget,
                              WidgetInputEvent& aEvent,
                              nsEventStatus& aStatus,
                              DispatchTo aDispatchTo);

  /**
   * StartCompositionAutomaticallyIfNecessary() starts composition if it hasn't
   * been started it yet.
   *
   * @param aStatus         If it succeeded to start composition normally, this
   *                        returns nsEventStatus_eIgnore.  Otherwise, e.g.,
   *                        the composition is canceled during dispatching
   *                        compositionstart event, this returns
   *                        nsEventStatus_eConsumeNoDefault.  In this case,
   *                        the caller shouldn't keep doing its job.
   * @return                Only when something unexpected occurs, this returns
   *                        an error.  Otherwise, returns NS_OK even if aStatus
   *                        is nsEventStatus_eConsumeNoDefault.
   */
  nsresult StartCompositionAutomaticallyIfNecessary(nsEventStatus& aStatus);

  /**
   * DispatchKeyboardEventInternal() maybe dispatches aKeyboardEvent.
   *
   * @param aMessage        Must be NS_KEY_DOWN, NS_KEY_UP or NS_KEY_PRESS.
   * @param aKeyboardEvent  A keyboard event.  If aMessage is NS_KEY_PRESS and
   *                        the event is for second or later character, its
   *                        mKeyValue should be empty string.
   * @param aStatus         If dispatching event should be marked as consumed,
   *                        set nsEventStatus_eConsumeNoDefault.  Otherwise,
   *                        set nsEventStatus_eIgnore.  After dispatching
   *                        a event and it's consumed this returns
   *                        nsEventStatus_eConsumeNoDefault.
   * @param aDispatchTo     See comments of DispatchTo.
   * @param aIndexOfKeypress    This must be 0 if aMessage isn't NS_KEY_PRESS or
   *                            aKeyboard.mKeyNameIndex isn't
   *                            KEY_NAME_INDEX_USE_STRING.  Otherwise, i.e.,
   *                            when an NS_KEY_PRESS event causes inputting
   *                            text, this must be between 0 and
   *                            mKeyValue.Length() - 1 since keypress events
   *                            sending only one character per event.
   * @return                true if an event is dispatched.  Otherwise, false.
   */
  bool DispatchKeyboardEventInternal(EventMessage aMessage,
                                     const WidgetKeyboardEvent& aKeyboardEvent,
                                     nsEventStatus& aStatus,
                                     DispatchTo aDispatchTo =
                                       eDispatchToParentProcess,
                                     uint32_t aIndexOfKeypress = 0);
};

} // namespace widget
} // namespace mozilla

#endif // #ifndef mozilla_widget_textcompositionsynthesizer_h_
