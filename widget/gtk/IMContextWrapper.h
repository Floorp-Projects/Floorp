/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IMContextWrapper_h_
#define IMContextWrapper_h_

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsIWidget.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/EventForwards.h"
#include "mozilla/TextEventDispatcherListener.h"
#include "WritingModes.h"

class nsWindow;

namespace mozilla {
namespace widget {

class IMContextWrapper final : public TextEventDispatcherListener
{
public:
    // TextEventDispatcherListener implementation
    NS_DECL_ISUPPORTS

    NS_IMETHOD NotifyIME(TextEventDispatcher* aTextEventDispatcher,
                         const IMENotification& aNotification) override;
    NS_IMETHOD_(void) OnRemovedFrom(
                          TextEventDispatcher* aTextEventDispatcher) override;
    NS_IMETHOD_(void) WillDispatchKeyboardEvent(
                          TextEventDispatcher* aTextEventDispatcher,
                          WidgetKeyboardEvent& aKeyboardEvent,
                          uint32_t aIndexOfKeypress,
                          void* aData) override;

public:
    // aOwnerWindow is a pointer of the owner window.  When aOwnerWindow is
    // destroyed, the related IME contexts are released (i.e., IME cannot be
    // used with the instance after that).
    explicit IMContextWrapper(nsWindow* aOwnerWindow);

    // "Enabled" means the users can use all IMEs.
    // I.e., the focus is in the normal editors.
    bool IsEnabled() const;

    nsIMEUpdatePreference GetIMEUpdatePreference() const;

    // OnFocusWindow is a notification that aWindow is going to be focused.
    void OnFocusWindow(nsWindow* aWindow);
    // OnBlurWindow is a notification that aWindow is going to be unfocused.
    void OnBlurWindow(nsWindow* aWindow);
    // OnDestroyWindow is a notification that aWindow is going to be destroyed.
    void OnDestroyWindow(nsWindow* aWindow);
    // OnFocusChangeInGecko is a notification that an editor gets focus.
    void OnFocusChangeInGecko(bool aFocus);
    // OnSelectionChange is a notification that selection (caret) is changed
    // in the focused editor.
    void OnSelectionChange(nsWindow* aCaller,
                           const IMENotification& aIMENotification);

    // OnKeyEvent is called when aWindow gets a native key press event or a
    // native key release event.  If this returns TRUE, the key event was
    // filtered by IME.  Otherwise, this returns FALSE.
    // NOTE: When the keypress event starts composition, this returns TRUE but
    //       this dispatches keydown event before compositionstart event.
    bool OnKeyEvent(nsWindow* aWindow, GdkEventKey* aEvent,
                      bool aKeyDownEventWasSent = false);

    // IME related nsIWidget methods.
    nsresult EndIMEComposition(nsWindow* aCaller);
    void SetInputContext(nsWindow* aCaller,
                         const InputContext* aContext,
                         const InputContextAction* aAction);
    InputContext GetInputContext();
    void OnUpdateComposition();
    void OnLayoutChange();

    TextEventDispatcher* GetTextEventDispatcher();

protected:
    ~IMContextWrapper();

    // Owner of an instance of this class. This should be top level window.
    // The owner window must release the contexts when it's destroyed because
    // the IME contexts need the native window.  If OnDestroyWindow() is called
    // with the owner window, it'll release IME contexts.  Otherwise, it'll
    // just clean up any existing composition if it's related to the destroying
    // child window.
    nsWindow* mOwnerWindow;

    // A last focused window in this class's context.
    nsWindow* mLastFocusedWindow;

    // Actual context. This is used for handling the user's input.
    GtkIMContext* mContext;

    // mSimpleContext is used for the password field and
    // the |ime-mode: disabled;| editors if sUseSimpleContext is true.
    // These editors disable IME.  But dead keys should work.  Fortunately,
    // the simple IM context of GTK2 support only them.
    GtkIMContext* mSimpleContext;

    // mDummyContext is a dummy context and will be used in Focus()
    // when the state of mEnabled means disabled.  This context's IME state is
    // always "closed", so it closes IME forcedly.
    GtkIMContext* mDummyContext;

    // mComposingContext is not nullptr while one of mContext, mSimpleContext
    // and mDummyContext has composition.
    // XXX: We don't assume that two or more context have composition same time.
    GtkIMContext* mComposingContext;

    // IME enabled state and other things defined in InputContext.
    // Use following helper methods if you don't need the detail of the status.
    InputContext mInputContext;

    // mCompositionStart is the start offset of the composition string in the
    // current content.  When <textarea> or <input> have focus, it means offset
    // from the first character of them.  When a HTML editor has focus, it
    // means offset from the first character of the root element of the editor.
    uint32_t mCompositionStart;

    // mDispatchedCompositionString is the latest composition string which
    // was dispatched by compositionupdate event.
    nsString mDispatchedCompositionString;

    // mSelectedString is the selected string which was removed by first
    // compositionchange event.
    nsString mSelectedString;

    // OnKeyEvent() temporarily sets mProcessingKeyEvent to the given native
    // event.
    GdkEventKey* mProcessingKeyEvent;

    struct Range
    {
        uint32_t mOffset;
        uint32_t mLength;

        Range()
            : mOffset(UINT32_MAX)
            , mLength(UINT32_MAX)
        {
        }

        bool IsValid() const { return mOffset != UINT32_MAX; }
        void Clear()
        {
            mOffset = UINT32_MAX;
            mLength = UINT32_MAX;
        }
    };

    // current target offset and length of IME composition
    Range mCompositionTargetRange;

    // mCompositionState indicates current status of composition.
    enum eCompositionState {
        eCompositionState_NotComposing,
        eCompositionState_CompositionStartDispatched,
        eCompositionState_CompositionChangeEventDispatched
    };
    eCompositionState mCompositionState;

    bool IsComposing() const
    {
        return (mCompositionState != eCompositionState_NotComposing);
    }

    bool IsComposingOn(GtkIMContext* aContext) const
    {
        return IsComposing() && mComposingContext == aContext;
    }

    bool IsComposingOnCurrentContext() const
    {
        return IsComposingOn(GetCurrentContext());
    }

    bool EditorHasCompositionString()
    {
        return (mCompositionState ==
                    eCompositionState_CompositionChangeEventDispatched);
    }

    /**
     * Checks if aContext is valid context for handling composition.
     *
     * @param aContext          An IM context which is specified by native
     *                          composition events.
     * @return                  true if the context is valid context for
     *                          handling composition.  Otherwise, false.
     */
    bool IsValidContext(GtkIMContext* aContext) const;

    const char* GetCompositionStateName()
    {
        switch (mCompositionState) {
            case eCompositionState_NotComposing:
                return "NotComposing";
            case eCompositionState_CompositionStartDispatched:
                return "CompositionStartDispatched";
            case eCompositionState_CompositionChangeEventDispatched:
                return "CompositionChangeEventDispatched";
            default:
                return "InvaildState";
        }
    }

    struct Selection final
    {
        uint32_t mOffset;
        uint32_t mLength;
        WritingMode mWritingMode;

        Selection()
            : mOffset(UINT32_MAX)
            , mLength(UINT32_MAX)
        {
        }

        void Clear()
        {
            mOffset = UINT32_MAX;
            mLength = UINT32_MAX;
            mWritingMode = WritingMode();
        }

        void Assign(const IMENotification& aIMENotification);
        void Assign(const WidgetQueryContentEvent& aSelectedTextEvent);

        bool IsValid() const { return mOffset != UINT32_MAX; }
        bool Collapsed() const { return !mLength; }
        uint32_t EndOffset() const
        {
            if (NS_WARN_IF(!IsValid())) {
                return UINT32_MAX;
            }
            CheckedInt<uint32_t> endOffset =
                CheckedInt<uint32_t>(mOffset) + mLength;
            if (NS_WARN_IF(!endOffset.isValid())) {
                return UINT32_MAX;
            }
            return endOffset.value();
        }
    } mSelection;
    bool EnsureToCacheSelection(nsAString* aSelectedString = nullptr);

    // mIsIMFocused is set to TRUE when we call gtk_im_context_focus_in(). And
    // it's set to FALSE when we call gtk_im_context_focus_out().
    bool mIsIMFocused;
    // mFilterKeyEvent is used by OnKeyEvent().  If the commit event should
    // be processed as simple key event, this is set to TRUE by the commit
    // handler.
    bool mFilterKeyEvent;
    // mKeyDownEventWasSent is used by OnKeyEvent() and
    // DispatchCompositionStart().  DispatchCompositionStart() dispatches
    // a keydown event if the composition start is caused by a native
    // keypress event.  If this is true, the keydown event has been dispatched.
    // Then, DispatchCompositionStart() doesn't dispatch keydown event.
    bool mKeyDownEventWasSent;
    // mIsDeletingSurrounding is true while OnDeleteSurroundingNative() is
    // trying to delete the surrounding text.
    bool mIsDeletingSurrounding;
    // mLayoutChanged is true after OnLayoutChange() is called.  This is reset
    // when eCompositionChange is being dispatched.
    bool mLayoutChanged;
    // mSetCursorPositionOnKeyEvent true when caret rect or position is updated
    // with no composition.  If true, we update candidate window position
    // before key down
    bool mSetCursorPositionOnKeyEvent;
    // mPendingResettingIMContext becomes true if selection change notification
    // is received during composition but the selection change occurred before
    // starting the composition.  In such case, we cannot notify IME of
    // selection change during composition because we don't want to commit
    // the composition in such case.  However, we should notify IME of the
    // selection change after the composition is committed.
    bool mPendingResettingIMContext;
    // mRetrieveSurroundingSignalReceived is true after "retrieve_surrounding"
    // signal is received until selection is changed in Gecko.
    bool mRetrieveSurroundingSignalReceived;

    // sLastFocusedContext is a pointer to the last focused instance of this
    // class.  When a instance is destroyed and sLastFocusedContext refers it,
    // this is cleared.  So, this refers valid pointer always.
    static IMContextWrapper* sLastFocusedContext;

    // sUseSimpleContext indeicates if password editors and editors with
    // |ime-mode: disabled;| should use GtkIMContextSimple.
    // If true, they use GtkIMContextSimple.  Otherwise, not.
    static bool sUseSimpleContext;

    // Callback methods for native IME events.  These methods should call
    // the related instance methods simply.
    static gboolean OnRetrieveSurroundingCallback(GtkIMContext* aContext,
                                                  IMContextWrapper* aModule);
    static gboolean OnDeleteSurroundingCallback(GtkIMContext* aContext,
                                                gint aOffset,
                                                gint aNChars,
                                                IMContextWrapper* aModule);
    static void OnCommitCompositionCallback(GtkIMContext* aContext,
                                            const gchar* aString,
                                            IMContextWrapper* aModule);
    static void OnChangeCompositionCallback(GtkIMContext* aContext,
                                            IMContextWrapper* aModule);
    static void OnStartCompositionCallback(GtkIMContext* aContext,
                                           IMContextWrapper* aModule);
    static void OnEndCompositionCallback(GtkIMContext* aContext,
                                         IMContextWrapper* aModule);

    // The instance methods for the native IME events.
    gboolean OnRetrieveSurroundingNative(GtkIMContext* aContext);
    gboolean OnDeleteSurroundingNative(GtkIMContext* aContext,
                                       gint aOffset,
                                       gint aNChars);
    void OnCommitCompositionNative(GtkIMContext* aContext,
                                   const gchar* aString);
    void OnChangeCompositionNative(GtkIMContext* aContext);
    void OnStartCompositionNative(GtkIMContext* aContext);
    void OnEndCompositionNative(GtkIMContext* aContext);

    /**
     * GetCurrentContext() returns current IM context which is chosen with the
     * enabled state.
     * WARNING:
     *     When this class receives some signals for a composition after focus
     *     is moved in Gecko, the result of this may be different from given
     *     context by the signals.
     */
    GtkIMContext* GetCurrentContext() const;

    /**
     * GetActiveContext() returns a composing context or current context.
     */
    GtkIMContext* GetActiveContext() const
    {
        return mComposingContext ? mComposingContext : GetCurrentContext();
    }

    // If the owner window and IM context have been destroyed, returns TRUE.
    bool IsDestroyed() { return !mOwnerWindow; }

    // Sets focus to the instance of this class.
    void Focus();

    // Steals focus from the instance of this class.
    void Blur();

    // Initializes the instance.
    void Init();

    // Reset the current composition of IME.  All native composition events
    // during this processing are ignored.
    void ResetIME();

    // Gets the current composition string by the native APIs.
    void GetCompositionString(GtkIMContext* aContext,
                              nsAString& aCompositionString);

    /**
     * Generates our text range array from current composition string.
     *
     * @param aContext              A GtkIMContext which is being handled.
     * @param aCompositionString    The data to be dispatched with
     *                              compositionchange event.
     */
    already_AddRefed<TextRangeArray>
        CreateTextRangeArray(GtkIMContext* aContext,
                             const nsAString& aCompositionString);

    /**
     * SetTextRange() initializes aTextRange with aPangoAttrIter.
     *
     * @param aPangoAttrIter            An iter which represents a clause of the
     *                                  composition string.
     * @param aUTF8CompositionString    The whole composition string (UTF-8).
     * @param aUTF16CaretOffset         The caret offset in the composition
     *                                  string encoded as UTF-16.
     * @param aTextRange                The result.
     * @return                          true if this initializes aTextRange.
     *                                  Otherwise, false.
     */
    bool SetTextRange(PangoAttrIterator* aPangoAttrIter,
                      const gchar* aUTF8CompositionString,
                      uint32_t aUTF16CaretOffset,
                      TextRange& aTextRange) const;

    /**
     * ToNscolor() converts the PangoColor in aPangoAttrColor to nscolor.
     */
    static nscolor ToNscolor(PangoAttrColor* aPangoAttrColor);

    /**
     * Move the candidate window with "fake" cursor position.
     *
     * @param aContext              A GtkIMContext which is being handled.
     */
    void SetCursorPosition(GtkIMContext* aContext);

    // Queries the current selection offset of the window.
    uint32_t GetSelectionOffset(nsWindow* aWindow);

    // Get current paragraph text content and cursor position
    nsresult GetCurrentParagraph(nsAString& aText, uint32_t& aCursorPos);

    /**
     * Delete text portion
     *
     * @param aContext              A GtkIMContext which is being handled.
     * @param aOffset               Start offset of the range to delete.
     * @param aNChars               Count of characters to delete.  It depends
     *                              on |g_utf8_strlen()| what is one character.
     */
    nsresult DeleteText(GtkIMContext* aContext,
                        int32_t aOffset,
                        uint32_t aNChars);

    // Initializes the GUI event.
    void InitEvent(WidgetGUIEvent& aEvent);

    // Called before destroying the context to work around some platform bugs.
    void PrepareToDestroyContext(GtkIMContext* aContext);

    /**
     *  WARNING:
     *    Following methods dispatch gecko events.  Then, the focused widget
     *    can be destroyed, and also it can be stolen focus.  If they returns
     *    FALSE, callers cannot continue the composition.
     *      - DispatchCompositionStart
     *      - DispatchCompositionChangeEvent
     *      - DispatchCompositionCommitEvent
     */

    /**
     * Dispatches a composition start event.
     *
     * @param aContext              A GtkIMContext which is being handled.
     * @return                      true if the focused widget is neither
     *                              destroyed nor changed.  Otherwise, false.
     */
    bool DispatchCompositionStart(GtkIMContext* aContext);

    /**
     * Dispatches a compositionchange event.
     *
     * @param aContext              A GtkIMContext which is being handled.
     * @param aCompositionString    New composition string.
     * @return                      true if the focused widget is neither
     *                              destroyed nor changed.  Otherwise, false.
     */
    bool DispatchCompositionChangeEvent(GtkIMContext* aContext,
                                        const nsAString& aCompositionString);

    /**
     * Dispatches a compositioncommit event or compositioncommitasis event.
     *
     * @param aContext              A GtkIMContext which is being handled.
     * @param aCommitString         If this is nullptr, the composition will
     *                              be committed with last dispatched data.
     *                              Otherwise, the composition will be
     *                              committed with this value.
     * @return                      true if the focused widget is neither
     *                              destroyed nor changed.  Otherwise, false.
     */
    bool DispatchCompositionCommitEvent(
             GtkIMContext* aContext,
             const nsAString* aCommitString = nullptr);
};

} // namespace widget
} // namespace mozilla

#endif // #ifndef IMContextWrapper_h_
