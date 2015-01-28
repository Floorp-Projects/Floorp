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
#include "mozilla/TextRange.h"

class nsIWidget;

namespace mozilla {
namespace widget {

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
   * need to call one of them before starting composition every time.  If they
   * return NS_ERROR_ALREADY_INITIALIZED, it means that another IME composes
   * with the instance.  Then, the caller shouldn't start composition.
   */
  nsresult Init();
  nsresult InitForTests();

  /**
   * OnDestroyWidget() is called when mWidget is being destroyed.
   */
  void OnDestroyWidget();

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

private:
  // mWidget is owner of the instance.  When this is created, this is set.
  // And when mWidget is released, this is cleared by OnDestroyWidget().
  // Note that mWidget may be destroyed already (i.e., mWidget->Destroyed() may
  // return true).
  nsIWidget* mWidget;

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
    nsresult Flush(const TextEventDispatcher* aDispatcher,
                   nsEventStatus& aStatus);
    void Clear();

  private:
    nsAutoString mString;
    nsRefPtr<TextRangeArray> mClauses;
    TextRange mCaret;

    void EnsureClauseArray();
  };
  PendingComposition mPendingComposition;

  bool mInitialized;
  bool mForTests;
};

} // namespace widget
} // namespace mozilla

#endif // #ifndef mozilla_widget_textcompositionsynthesizer_h_
