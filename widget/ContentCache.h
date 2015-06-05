/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ContentCache_h
#define mozilla_ContentCache_h

#include <stdint.h>

#include "mozilla/Assertions.h"
#include "mozilla/EventForwards.h"
#include "nsString.h"

class nsIWidget;

namespace mozilla {

/**
 * ContentCache stores various information of the child content.  This hides
 * raw information but you can access more useful information with a lot of
 * methods.
 */

class ContentCache final
{
public:
  ContentCache();

  void Clear();

  void SetText(const nsAString& aText);
  const nsString& Text() const { return mText; }
  uint32_t TextLength() const { return mText.Length(); }

  /**
   * OnCompositionEvent() should be called before sending composition string.
   * This returns true if the event should be sent.  Otherwise, false.
   */
  bool OnCompositionEvent(const WidgetCompositionEvent& aCompositionEvent);
  /**
   * RequestToCommitComposition() requests to commit or cancel composition to
   * the widget.  If it's handled synchronously, this returns the number of
   * composition events after that.
   *
   * @param aWidget     The widget to be requested to commit or cancel
   *                    the composition.
   * @param aCancel     When the caller tries to cancel the composition, true.
   *                    Otherwise, i.e., tries to commit the composition, false.
   * @param aLastString The last composition string before requesting to
   *                    commit or cancel composition.
   * @return            The count of composition events ignored after a call of
   *                    WillRequestToCommitOrCancelComposition().
   */
  uint32_t RequestToCommitComposition(nsIWidget* aWidget,
                                      bool aCancel,
                                      nsAString& aLastString);

  void SetSelection(uint32_t aCaretOffset)
  {
    SetSelection(aCaretOffset, aCaretOffset);
  }
  void SetSelection(uint32_t aAnchorOffset, uint32_t aFocusOffset);
  bool SelectionCollapsed() const { return mSelection.Collapsed(); }
  bool SelectionReversed() const { return mSelection.Reversed(); }
  bool SelectionEndIsGraterThanTextLength() const
  {
    return SelectionEnd() > TextLength();
  }
  uint32_t SelectionAnchor() const { return mSelection.mAnchor; }
  uint32_t SelectionFocus() const { return mSelection.mFocus; }
  uint32_t SelectionStart() const { return mSelection.StartOffset(); }
  uint32_t SelectionEnd() const { return mSelection.EndOffset(); }
  uint32_t SelectionLength() const { return mSelection.Length(); }

private:
  // Whole text in the target
  nsString mText;
  // This is commit string which is caused by our request.
  nsString mCommitStringByRequest;
  // Start offset of the composition string.
  uint32_t mCompositionStart;
  // Count of composition events during requesting commit or cancel the
  // composition.
  uint32_t mCompositionEventsDuringRequest;

  struct Selection final
  {
    // Following values are offset in "flat text".
    uint32_t mAnchor;
    uint32_t mFocus;

    Selection()
      : mAnchor(0)
      , mFocus(0)
    {
    }

    bool Collapsed() const { return mFocus == mAnchor; }
    bool Reversed() const { return mFocus < mAnchor; }
    uint32_t StartOffset() const { return Reversed() ? mFocus : mAnchor; }
    uint32_t EndOffset() const { return Reversed() ? mAnchor : mFocus; }
    uint32_t Length() const
    {
      return Reversed() ? mAnchor - mFocus : mFocus - mAnchor;
    }
  } mSelection;

  bool mIsComposing;
  bool mRequestedToCommitOrCancelComposition;
};

} // namespace mozilla

#endif // mozilla_ContentCache_h
