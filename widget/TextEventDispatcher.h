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
 * WidgetCompositionEvent.  However, WidgetKeyboardEvent and/or
 * WidgetQueryContentEvent may be supported by this class in the future.
 * An instance of this class is created by nsIWidget instance and owned by it.
 * This is typically created only by the top level widgets because only they
 * handle IME.
 */

class TextEventDispatcher MOZ_FINAL
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
   * @see nsIWidget::NotifyIME()
   */
  nsresult NotifyIME(const IMENotification& aIMENotification);

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

  bool mForTests;
  // See IsComposing().
  bool mIsComposing;

  nsresult BeginInputTransactionInternal(
             TextEventDispatcherListener* aListener,
             bool aForTests);

  /**
   * InitEvent() initializes aEvent.  This must be called before dispatching
   * the event.
   */
  void InitEvent(WidgetCompositionEvent& aEvent) const;

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
};

} // namespace widget
} // namespace mozilla

#endif // #ifndef mozilla_widget_textcompositionsynthesizer_h_
