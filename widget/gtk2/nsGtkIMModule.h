/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Christopher Blizzard
 * <blizzard@mozilla.org>.  Portions created by the Initial Developer
 * are Copyright (C) 2001 the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __nsGtkIMModule_h__
#define __nsGtkIMModule_h__

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "nsString.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"
#include "nsGUIEvent.h"
#include "nsIWidget.h"

// If software keyboard is needed in password field and uses GTK2 IM module
// for inputting characters, we need to enable IME in password field too.
#ifdef MOZ_PLATFORM_MAEMO
#define NS_IME_ENABLED_ON_PASSWORD_FIELD 1
#endif

class nsWindow;

class nsGtkIMModule
{
protected:
    typedef mozilla::widget::InputContext InputContext;
    typedef mozilla::widget::InputContextAction InputContextAction;

public:
    nsrefcnt AddRef()
    {
        NS_PRECONDITION(PRInt32(mRefCnt) >= 0, "mRefCnt is negative");
        ++mRefCnt;
        NS_LOG_ADDREF(this, mRefCnt, "nsGtkIMModule", sizeof(*this));
        return mRefCnt;
    }
    nsrefcnt Release()
    {
        NS_PRECONDITION(mRefCnt != 0, "mRefCnt is alrady zero");
        --mRefCnt;
        NS_LOG_RELEASE(this, mRefCnt, "nsGtkIMModule");
        if (mRefCnt == 0) {
            mRefCnt = 1; /* stabilize */
            delete this;
            return 0;
        }
        return mRefCnt;
    }

protected:
    nsAutoRefCnt mRefCnt;

public:
    // aOwnerWindow is a pointer of the owner window.  When aOwnerWindow is
    // destroyed, the related IME contexts are released (i.e., IME cannot be
    // used with the instance after that).
    nsGtkIMModule(nsWindow* aOwnerWindow);
    ~nsGtkIMModule();

    // "Enabled" means the users can use all IMEs.
    // I.e., the focus is in the normal editors.
    bool IsEnabled();

    // OnFocusWindow is a notification that aWindow is going to be focused.
    void OnFocusWindow(nsWindow* aWindow);
    // OnBlurWindow is a notification that aWindow is going to be unfocused.
    void OnBlurWindow(nsWindow* aWindow);
    // OnDestroyWindow is a notification that aWindow is going to be destroyed.
    void OnDestroyWindow(nsWindow* aWindow);
    // OnFocusChangeInGecko is a notification that an editor gets focus.
    void OnFocusChangeInGecko(bool aFocus);

    // OnKeyEvent is called when aWindow gets a native key press event or a
    // native key release event.  If this returns TRUE, the key event was
    // filtered by IME.  Otherwise, this returns FALSE.
    // NOTE: When the keypress event starts composition, this returns TRUE but
    //       this dispatches keydown event before compositionstart event.
    bool OnKeyEvent(nsWindow* aWindow, GdkEventKey* aEvent,
                      bool aKeyDownEventWasSent = false);

    // IME related nsIWidget methods.
    nsresult ResetInputState(nsWindow* aCaller);
    void SetInputContext(nsWindow* aCaller,
                         const InputContext* aContext,
                         const InputContextAction* aAction);
    InputContext GetInputContext();
    nsresult CancelIMEComposition(nsWindow* aCaller);

    // If a software keyboard has been opened, this returns TRUE.
    // Otherwise, FALSE.
    static bool IsVirtualKeyboardOpened();

protected:
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
    GtkIMContext       *mContext;

#ifndef NS_IME_ENABLED_ON_PASSWORD_FIELD
    // mSimpleContext is used for the password field and
    // the |ime-mode: disabled;| editors.  These editors disable IME.
    // But dead keys should work.  Fortunately, the simple IM context of
    // GTK2 support only them.
    GtkIMContext       *mSimpleContext;
#endif // NS_IME_ENABLED_ON_PASSWORD_FIELD

    // mDummyContext is a dummy context and will be used in Focus()
    // when the state of mEnabled means disabled.  This context's IME state is
    // always "closed", so it closes IME forcedly.
    GtkIMContext       *mDummyContext;

    // IME enabled state and other things defined in InputContext.
    // Use following helper methods if you don't need the detail of the status.
    InputContext mInputContext;

    // mCompositionStart is the start offset of the composition string in the
    // current content.  When <textarea> or <input> have focus, it means offset
    // from the first character of them.  When a HTML editor has focus, it
    // means offset from the first character of the root element of the editor.
    PRUint32 mCompositionStart;

    // mDispatchedCompositionString is the latest composition string which
    // was dispatched by compositionupdate event.
    nsString mDispatchedCompositionString;

    // mSelectedString is the selected string which was removed by first
    // text event.
    nsString mSelectedString;

    // OnKeyEvent() temporarily sets mProcessingKeyEvent to the given native
    // event.
    GdkEventKey* mProcessingKeyEvent;

    // mCompositionState indicates current status of composition.
    enum eCompositionState {
        eCompositionState_NotComposing,
        eCompositionState_CompositionStartDispatched,
        eCompositionState_TextEventDispatched,
        eCompositionState_CommitTextEventDispatched
    };
    eCompositionState mCompositionState;

    bool IsComposing()
    {
        return (mCompositionState != eCompositionState_NotComposing);
    }

    bool EditorHasCompositionString()
    {
        return (mCompositionState == eCompositionState_TextEventDispatched);
    }

#ifdef PR_LOGGING
    const char* GetCompositionStateName()
    {
        switch (mCompositionState) {
            case eCompositionState_NotComposing:
                return "NotComposing";
            case eCompositionState_CompositionStartDispatched:
                return "CompositionStartDispatched";
            case eCompositionState_TextEventDispatched:
                return "TextEventDispatched";
            case eCompositionState_CommitTextEventDispatched:
                return "CommitTextEventDispatched";
            default:
                return "InvaildState";
        }
    }
#endif // PR_LOGGING


    // mIsIMFocused is set to TRUE when we call gtk_im_context_focus_in(). And
    // it's set to FALSE when we call gtk_im_context_focus_out().
    bool mIsIMFocused;
    // mFilterKeyEvent is used by OnKeyEvent().  If the commit event should
    // be processed as simple key event, this is set to TRUE by the commit
    // handler.
    bool mFilterKeyEvent;
    // When mIgnoreNativeCompositionEvent is TRUE, all native composition
    // should be ignored except that the compositon should be restarted in
    // another content (nsIContent).  Don't refer this value directly, use
    // ShouldIgnoreNativeCompositionEvent().
    bool mIgnoreNativeCompositionEvent;
    // mKeyDownEventWasSent is used by OnKeyEvent() and
    // DispatchCompositionStart().  DispatchCompositionStart() dispatches
    // a keydown event if the composition start is caused by a native
    // keypress event.  If this is true, the keydown event has been dispatched.
    // Then, DispatchCompositionStart() doesn't dispatch keydown event.
    bool mKeyDownEventWasSent;

    // sLastFocusedModule is a pointer to the last focused instance of this
    // class.  When a instance is destroyed and sLastFocusedModule refers it,
    // this is cleared.  So, this refers valid pointer always.
    static nsGtkIMModule* sLastFocusedModule;

    // Callback methods for native IME events.  These methods should call
    // the related instance methods simply.
    static gboolean OnRetrieveSurroundingCallback(GtkIMContext  *aContext,
                                                  nsGtkIMModule *aModule);
    static gboolean OnDeleteSurroundingCallback(GtkIMContext  *aContext,
                                                gint           aOffset,
                                                gint           aNChars,
                                                nsGtkIMModule *aModule);
    static void OnCommitCompositionCallback(GtkIMContext *aContext,
                                            const gchar *aString,
                                            nsGtkIMModule* aModule);
    static void OnChangeCompositionCallback(GtkIMContext *aContext,
                                            nsGtkIMModule* aModule);
    static void OnStartCompositionCallback(GtkIMContext *aContext,
                                           nsGtkIMModule* aModule);
    static void OnEndCompositionCallback(GtkIMContext *aContext,
                                         nsGtkIMModule* aModule);

    // The instance methods for the native IME events.
    gboolean OnRetrieveSurroundingNative(GtkIMContext  *aContext);
    gboolean OnDeleteSurroundingNative(GtkIMContext  *aContext,
                                       gint           aOffset,
                                       gint           aNChars);
    void OnCommitCompositionNative(GtkIMContext *aContext,
                                   const gchar *aString);
    void OnChangeCompositionNative(GtkIMContext *aContext);
    void OnStartCompositionNative(GtkIMContext *aContext);
    void OnEndCompositionNative(GtkIMContext *aContext);


    // GetContext() returns current IM context which is chosen by the enabled
    // state.  So, this means *current* IM context.
    GtkIMContext* GetContext();

    // "Editable" means the users can input characters. They may be not able to
    // use IMEs but they can use dead keys.
    // I.e., the focus is in the normal editors or the password editors or
    // the |ime-mode: disabled;| editors.
    bool IsEditable();

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
    void GetCompositionString(nsAString &aCompositionString);

    // Generates our text range list from current composition string.
    void SetTextRangeList(nsTArray<nsTextRange> &aTextRangeList);

    // Sets the offset's cursor position to IME.
    void SetCursorPosition(PRUint32 aTargetOffset);

    // Queries the current selection offset of the window.
    PRUint32 GetSelectionOffset(nsWindow* aWindow);

    // Get current paragraph text content and cursor position
    nsresult GetCurrentParagraph(nsAString& aText, PRUint32& aCursorPos);

    // Delete text portion
    nsresult DeleteText(const PRInt32 aOffset, const PRUint32 aNChars);

    // Initializes the GUI event.
    void InitEvent(nsGUIEvent& aEvent);

    // Called before destroying the context to work around some platform bugs.
    void PrepareToDestroyContext(GtkIMContext *aContext);

    bool ShouldIgnoreNativeCompositionEvent();

    /**
     *  WARNING:
     *    Following methods dispatch gecko events.  Then, the focused widget
     *    can be destroyed, and also it can be stolen focus.  If they returns
     *    FALSE, callers cannot continue the composition.
     *      - CommitCompositionBy
     *      - DispatchCompositionStart
     *      - DispatchCompositionEnd
     *      - DispatchTextEvent
     */

    // Commits the current composition by the aString.
    bool CommitCompositionBy(const nsAString& aString);

    // Dispatches a composition start event or a composition end event.
    bool DispatchCompositionStart();
    bool DispatchCompositionEnd();

    // Dispatches a text event.  If aIsCommit is TRUE, dispatches a committed
    // text event.  Otherwise, dispatches a composing text event.
    bool DispatchTextEvent(const nsAString& aCompositionString,
                           bool aIsCommit);

};

#endif // __nsGtkIMModule_h__
