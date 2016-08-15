/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.lang.reflect.Array;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.Semaphore;

import org.json.JSONObject;
import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.mozglue.JNIObject;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.ThreadUtils.AssertBehavior;

import android.graphics.RectF;
import android.os.Handler;
import android.os.Looper;
import android.text.Editable;
import android.text.InputFilter;
import android.text.NoCopySpan;
import android.text.Selection;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.TextPaint;
import android.text.TextUtils;
import android.text.style.CharacterStyle;
import android.util.Log;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;

/*
   GeckoEditable implements only some functions of Editable
   The field mText contains the actual underlying
   SpannableStringBuilder/Editable that contains our text.
*/
final class GeckoEditable extends JNIObject
        implements InvocationHandler, Editable,
                   GeckoEditableClient, GeckoEditableListener, GeckoEventListener {

    private static final boolean DEBUG = false;
    private static final String LOGTAG = "GeckoEditable";

    // Filters to implement Editable's filtering functionality
    private InputFilter[] mFilters;

    private final SpannableStringBuilder mText;
    private final Editable mProxy;
    private final ActionQueue mActionQueue;

    // mIcRunHandler is the Handler that currently runs Gecko-to-IC Runnables
    // mIcPostHandler is the Handler to post Gecko-to-IC Runnables to
    // The two can be different when switching from one handler to another
    private Handler mIcRunHandler;
    private Handler mIcPostHandler;

    /* package */ GeckoEditableListener mListener;
    /* package */ GeckoView mView;
    /* package */ boolean mInBatchMode; // Used by IC thread
    /* package */ boolean mNeedCompositionUpdate; // Used by IC thread
    private boolean mFocused; // Used by IC thread
    private boolean mGeckoFocused; // Used by Gecko thread
    private boolean mIgnoreSelectionChange; // Used by Gecko thread
    private volatile boolean mSuppressCompositions;
    private volatile boolean mSuppressKeyUp;

    private static final int IME_RANGE_CARETPOSITION = 1;
    private static final int IME_RANGE_RAWINPUT = 2;
    private static final int IME_RANGE_SELECTEDRAWTEXT = 3;
    private static final int IME_RANGE_CONVERTEDTEXT = 4;
    private static final int IME_RANGE_SELECTEDCONVERTEDTEXT = 5;

    private static final int IME_RANGE_LINE_NONE = 0;
    private static final int IME_RANGE_LINE_DOTTED = 1;
    private static final int IME_RANGE_LINE_DASHED = 2;
    private static final int IME_RANGE_LINE_SOLID = 3;
    private static final int IME_RANGE_LINE_DOUBLE = 4;
    private static final int IME_RANGE_LINE_WAVY = 5;

    private static final int IME_RANGE_UNDERLINE = 1;
    private static final int IME_RANGE_FORECOLOR = 2;
    private static final int IME_RANGE_BACKCOLOR = 4;
    private static final int IME_RANGE_LINECOLOR = 8;

    @WrapForJNI(dispatchTo = "proxy")
    private native void onKeyEvent(int action, int keyCode, int scanCode, int metaState,
                                   long time, int unicodeChar, int baseUnicodeChar,
                                   int domPrintableKeyValue, int repeatCount, int flags,
                                   boolean isSynthesizedImeKey, KeyEvent event);

    private void onKeyEvent(KeyEvent event, int action, int savedMetaState,
                            boolean isSynthesizedImeKey) {
        // Use a separate action argument so we can override the key's original action,
        // e.g. change ACTION_MULTIPLE to ACTION_DOWN. That way we don't have to allocate
        // a new key event just to change its action field.
        //
        // Normally we expect event.getMetaState() to reflect the current meta-state; however,
        // some software-generated key events may not have event.getMetaState() set, e.g. key
        // events from Swype. Therefore, it's necessary to combine the key's meta-states
        // with the meta-states that we keep separately in KeyListener
        final int metaState = event.getMetaState() | savedMetaState;
        final int unmodifiedMetaState = metaState &
                ~(KeyEvent.META_ALT_MASK | KeyEvent.META_CTRL_MASK | KeyEvent.META_META_MASK);
        final int unicodeChar = event.getUnicodeChar(metaState);
        final int domPrintableKeyValue =
                unicodeChar >= ' '               ? unicodeChar :
                unmodifiedMetaState != metaState ? event.getUnicodeChar(unmodifiedMetaState) :
                                                   0;
        onKeyEvent(action, event.getKeyCode(), event.getScanCode(),
                   metaState, event.getEventTime(), unicodeChar,
                   // e.g. for Ctrl+A, Android returns 0 for unicodeChar,
                   // but Gecko expects 'a', so we return that in baseUnicodeChar.
                   event.getUnicodeChar(0), domPrintableKeyValue, event.getRepeatCount(),
                   event.getFlags(), isSynthesizedImeKey, event);
    }

    @WrapForJNI(dispatchTo = "proxy")
    private native void onImeSynchronize();

    @WrapForJNI(dispatchTo = "proxy")
    private native void onImeAcknowledgeFocus();

    @WrapForJNI(dispatchTo = "proxy")
    private native void onImeReplaceText(int start, int end, String text);

    @WrapForJNI(dispatchTo = "proxy")
    private native void onImeAddCompositionRange(int start, int end, int rangeType,
                                                 int rangeStyles, int rangeLineStyle,
                                                 boolean rangeBoldLine, int rangeForeColor,
                                                 int rangeBackColor, int rangeLineColor);

    @WrapForJNI(dispatchTo = "proxy")
    private native void onImeUpdateComposition(int start, int end);

    @WrapForJNI(dispatchTo = "proxy")
    private native void onImeRequestCursorUpdates(int requestMode);

    /* An action that alters the Editable

       Each action corresponds to a Gecko event. While the Gecko event is being sent to the Gecko
       thread, the action stays on top of mActions queue. After the Gecko event is processed and
       replied, the action is removed from the queue
    */
    private static final class Action {
        // For input events (keypress, etc.); use with onImeSynchronize
        static final int TYPE_EVENT = 0;
        // For Editable.replace() call; use with onImeReplaceText
        static final int TYPE_REPLACE_TEXT = 1;
        // For Editable.setSpan() call; use with onImeSynchronize
        static final int TYPE_SET_SPAN = 2;
        // For Editable.removeSpan() call; use with onImeSynchronize
        static final int TYPE_REMOVE_SPAN = 3;
        // For focus events (in notifyIME); use with onImeAcknowledgeFocus
        static final int TYPE_ACKNOWLEDGE_FOCUS = 4;
        // For switching handler; use with onImeSynchronize
        static final int TYPE_SET_HANDLER = 5;
        // For updating composition; use with onImeUpdateComposition
        static final int TYPE_UPDATE_COMPOSITION = 6;

        final int mType;
        boolean mUpdateComposition;
        int mStart;
        int mEnd;
        CharSequence mSequence;
        Object mSpanObject;
        int mSpanFlags;
        Handler mHandler;

        Action(int type) {
            mType = type;
        }

        static Action newReplaceText(CharSequence text, int start, int end) {
            if (start < 0 || start > end) {
                Log.e(LOGTAG, "invalid replace text offsets: " + start + " to " + end);
                throw new IllegalArgumentException("invalid replace text offsets");
            }

            final Action action = new Action(TYPE_REPLACE_TEXT);
            action.mSequence = text;
            action.mStart = start;
            action.mEnd = end;
            return action;
        }

        static Action newSetSpan(Object object, int start, int end, int flags, boolean update) {
            if (start < 0 || start > end) {
                Log.e(LOGTAG, "invalid span offsets: " + start + " to " + end);
                throw new IllegalArgumentException("invalid span offsets");
            }
            final Action action = new Action(TYPE_SET_SPAN);
            action.mSpanObject = object;
            action.mStart = start;
            action.mEnd = end;
            action.mSpanFlags = flags;
            action.mUpdateComposition = update;
            return action;
        }

        static Action newRemoveSpan(Object object, boolean update) {
            final Action action = new Action(TYPE_REMOVE_SPAN);
            action.mSpanObject = object;
            action.mUpdateComposition = update;
            return action;
        }

        static Action newSetHandler(Handler handler) {
            final Action action = new Action(TYPE_SET_HANDLER);
            action.mHandler = handler;
            return action;
        }

        static Action newUpdateComposition(int start, int end) {
            final Action action = new Action(TYPE_UPDATE_COMPOSITION);
            action.mStart = start;
            action.mEnd = end;
            return action;
        }
    }

    /* Queue of editing actions sent to Gecko thread that
       the Gecko thread has not responded to yet */
    private final class ActionQueue {
        private final ConcurrentLinkedQueue<Action> mActions;
        private final Semaphore mActionsActive;
        private KeyCharacterMap mKeyMap;

        ActionQueue() {
            mActions = new ConcurrentLinkedQueue<Action>();
            mActionsActive = new Semaphore(1);
        }

        void offer(Action action) {
            if (DEBUG) {
                assertOnIcThread();
                Log.d(LOGTAG, "offer: Action(" +
                              getConstantName(Action.class, "TYPE_", action.mType) + ")");
            }

            if (mListener == null) {
                // We haven't initialized or we've been destroyed.
                return;
            }

            if (mActions.isEmpty()) {
                mActionsActive.acquireUninterruptibly();
                mActions.offer(action);
            } else synchronized (this) {
                // tryAcquire here in case Gecko thread has just released it
                mActionsActive.tryAcquire();
                mActions.offer(action);
            }

            switch (action.mType) {
            case Action.TYPE_EVENT:
            case Action.TYPE_SET_SPAN:
            case Action.TYPE_REMOVE_SPAN:
            case Action.TYPE_SET_HANDLER:
                onImeSynchronize();
                break;

            case Action.TYPE_REPLACE_TEXT:
                // Because we get composition styling here essentially for free,
                // we don't need to check if we're in batch mode.
                if (icMaybeSendComposition(
                        action.mSequence, /* useEntireText */ true, /* notifyGecko */ false)) {
                    mNeedCompositionUpdate = false;
                } else {
                    // Since we don't have a composition, we can try sending key events.
                    sendCharKeyEvents(action);
                }
                onImeReplaceText(action.mStart, action.mEnd, action.mSequence.toString());
                break;

            case Action.TYPE_ACKNOWLEDGE_FOCUS:
                onImeAcknowledgeFocus();
                break;

            case Action.TYPE_UPDATE_COMPOSITION:
                onImeUpdateComposition(action.mStart, action.mEnd);
                break;

            default:
                throw new IllegalStateException("Action not processed");
            }
        }

        private KeyEvent [] synthesizeKeyEvents(CharSequence cs) {
            try {
                if (mKeyMap == null) {
                    mKeyMap = KeyCharacterMap.load(KeyCharacterMap.VIRTUAL_KEYBOARD);
                }
            } catch (Exception e) {
                // KeyCharacterMap.UnavailableException is not found on Gingerbread;
                // besides, it seems like HC and ICS will throw something other than
                // KeyCharacterMap.UnavailableException; so use a generic Exception here
                return null;
            }
            KeyEvent [] keyEvents = mKeyMap.getEvents(cs.toString().toCharArray());
            if (keyEvents == null || keyEvents.length == 0) {
                return null;
            }
            return keyEvents;
        }

        private void sendCharKeyEvents(Action action) {
            if (action.mSequence.length() != 1 ||
                (action.mSequence instanceof Spannable &&
                ((Spannable)action.mSequence).nextSpanTransition(
                    -1, Integer.MAX_VALUE, null) < Integer.MAX_VALUE)) {
                // Spans are not preserved when we use key events,
                // so we need the sequence to not have any spans
                return;
            }
            KeyEvent [] keyEvents = synthesizeKeyEvents(action.mSequence);
            if (keyEvents == null) {
                return;
            }
            for (KeyEvent event : keyEvents) {
                if (KeyEvent.isModifierKey(event.getKeyCode())) {
                    continue;
                }
                if (event.getAction() == KeyEvent.ACTION_UP && mSuppressKeyUp) {
                    continue;
                }
                if (DEBUG) {
                    Log.d(LOGTAG, "sending: " + event);
                }
                onKeyEvent(event, event.getAction(),
                           /* metaState */ 0, /* isSynthesizedImeKey */ true);
            }
        }

        /**
         * Remove the head of the queue. Throw if queue is empty.
         */
        void poll() {
            if (DEBUG) {
                ThreadUtils.assertOnGeckoThread();
            }
            if (mActions.poll() == null) {
                throw new IllegalStateException("empty actions queue");
            }

            synchronized (this) {
                if (mActions.isEmpty()) {
                    mActionsActive.release();
                }
            }
        }

        /**
         * Return, but don't remove, the head of the queue, or null if queue is empty.
         *
         * @return head of the queue or null if empty.
         */
        Action peek() {
            if (DEBUG) {
                ThreadUtils.assertOnGeckoThread();
            }
            return mActions.peek();
        }

        void syncWithGecko() {
            if (DEBUG) {
                assertOnIcThread();
            }
            if (mFocused && !mActions.isEmpty()) {
                if (DEBUG) {
                    Log.d(LOGTAG, "syncWithGecko blocking on thread " +
                                  Thread.currentThread().getName());
                }
                mActionsActive.acquireUninterruptibly();
                mActionsActive.release();
            } else if (DEBUG && !mFocused) {
                Log.d(LOGTAG, "skipped syncWithGecko (no focus)");
            }
        }

        boolean isEmpty() {
            return mActions.isEmpty();
        }
    }

    @WrapForJNI(calledFrom = "gecko")
    GeckoEditable(final GeckoView v) {
        if (DEBUG) {
            // Called by nsWindow.
            ThreadUtils.assertOnGeckoThread();
        }
        mActionQueue = new ActionQueue();

        mText = new SpannableStringBuilder();

        final Class<?>[] PROXY_INTERFACES = { Editable.class };
        mProxy = (Editable)Proxy.newProxyInstance(
                Editable.class.getClassLoader(),
                PROXY_INTERFACES, this);

        mIcRunHandler = mIcPostHandler = ThreadUtils.getUiHandler();

        onViewChange(v);
    }

    @WrapForJNI(dispatchTo = "proxy") @Override
    protected native void disposeNative();

    @WrapForJNI(calledFrom = "gecko")
    /* package */ void onViewChange(final GeckoView v) {
        if (DEBUG) {
            // Called by nsWindow.
            ThreadUtils.assertOnGeckoThread();
            Log.d(LOGTAG, "onViewChange(" + v + ")");
        }

        final GeckoEditableListener newListener =
            v != null ? GeckoInputConnection.create(v, this) : null;

        final Runnable setListenerRunnable = new Runnable() {
            @Override
            public void run() {
                if (DEBUG) {
                    Log.d(LOGTAG, "onViewChange (set listener)");
                }

                if (newListener != null) {
                    // Make sure there are no other things going on.
                    mActionQueue.syncWithGecko();
                    mListener = newListener;
                } else {
                    // We're being destroyed. By this point, we should have cleared all
                    // pending Runnables on the IC thread, so it's safe to call
                    // disposeNative here.
                    mListener = null;
                    GeckoEditable.this.disposeNative();
                }
            }
        };

        // Post to UI thread first to make sure any code that is using the old input
        // connection has finished running, before we switch to a new input connection or
        // before we clear the input connection on destruction.
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                if (DEBUG) {
                    Log.d(LOGTAG, "onViewChange (set IC)");
                }

                if (mView != null) {
                    // Detach the previous view.
                    mView.setInputConnectionListener(null);
                }
                if (v != null) {
                    // And attach the new view.
                    v.setInputConnectionListener((InputConnectionListener) newListener);
                }

                mView = v;
                mIcPostHandler.post(setListenerRunnable);
            }
        });
    }

    private boolean onIcThread() {
        return mIcRunHandler.getLooper() == Looper.myLooper();
    }

    private void assertOnIcThread() {
        ThreadUtils.assertOnThread(mIcRunHandler.getLooper().getThread(), AssertBehavior.THROW);
    }

    private void geckoPostToIc(Runnable runnable) {
        mIcPostHandler.post(runnable);
    }

    private Object getField(Object obj, String field, Object def) {
        try {
            return obj.getClass().getField(field).get(obj);
        } catch (Exception e) {
            return def;
        }
    }

    /**
     * Send composition ranges to Gecko if the text has composing spans.
     *
     * @param sequence Text with possible composing spans
     * @param useEntireText If text has composing spans, treat the entire text as
     *                      a Gecko composition, instead of just the spanned part.
     * @param notifyGecko Notify Gecko of the new composition ranges;
     *                    otherwise, the caller is responsible for notifying Gecko.
     * @return Whether there was a composition
     */
    private boolean icMaybeSendComposition(final CharSequence sequence,
                                           final boolean useEntireText,
                                           final boolean notifyGecko) {
        int selStart = Selection.getSelectionStart(sequence);
        int selEnd = Selection.getSelectionEnd(sequence);

        if (sequence instanceof Spanned) {
            final Spanned text = (Spanned) sequence;
            final Object[] spans = text.getSpans(0, text.length(), Object.class);
            boolean found = false;
            int composingStart = useEntireText ? 0 : Integer.MAX_VALUE;
            int composingEnd = useEntireText ? text.length() : 0;

            for (Object span : spans) {
                if ((text.getSpanFlags(span) & Spanned.SPAN_COMPOSING) == 0) {
                    continue;
                }
                if (!useEntireText) {
                    composingStart = Math.min(composingStart, text.getSpanStart(span));
                    composingEnd = Math.max(composingEnd, text.getSpanEnd(span));
                }
                found = true;
            }

            if (useEntireText && (selStart < 0 || selEnd < 0)) {
                selStart = composingEnd;
                selEnd = composingEnd;
            }

            if (found) {
                icSendComposition(text, selStart, selEnd, composingStart, composingEnd);
                if (notifyGecko) {
                    mActionQueue.offer(Action.newUpdateComposition(
                            composingStart, composingEnd));
                }
                return true;
            }
        }

        if (notifyGecko) {
            // Set the selection by using a composition without ranges
            mActionQueue.offer(Action.newUpdateComposition(selStart, selEnd));
        }

        if (DEBUG) {
            Log.d(LOGTAG, "icSendComposition(): no composition");
        }
        return false;
    }

    private void icSendComposition(final Spanned text,
                                   final int selStart, final int selEnd,
                                   final int composingStart, final int composingEnd) {
        if (DEBUG) {
            assertOnIcThread();
            Log.d(LOGTAG, "icSendComposition(\"" + text + "\", " +
                                             composingStart + ", " + composingEnd + ")");
        }
        if (DEBUG) {
            Log.d(LOGTAG, " range = " + composingStart + "-" + composingEnd);
            Log.d(LOGTAG, " selection = " + selStart + "-" + selEnd);
        }

        if (selEnd >= composingStart && selEnd <= composingEnd) {
            onImeAddCompositionRange(
                    selEnd - composingStart, selEnd - composingStart,
                    IME_RANGE_CARETPOSITION, 0, 0, false, 0, 0, 0);
        }

        int rangeStart = composingStart;
        TextPaint tp = new TextPaint();
        TextPaint emptyTp = new TextPaint();
        // set initial foreground color to 0, because we check for tp.getColor() == 0
        // below to decide whether to pass a foreground color to Gecko
        emptyTp.setColor(0);
        do {
            int rangeType, rangeStyles = 0, rangeLineStyle = IME_RANGE_LINE_NONE;
            boolean rangeBoldLine = false;
            int rangeForeColor = 0, rangeBackColor = 0, rangeLineColor = 0;
            int rangeEnd = text.nextSpanTransition(rangeStart, composingEnd, Object.class);

            if (selStart > rangeStart && selStart < rangeEnd) {
                rangeEnd = selStart;
            } else if (selEnd > rangeStart && selEnd < rangeEnd) {
                rangeEnd = selEnd;
            }
            CharacterStyle[] styleSpans =
                    text.getSpans(rangeStart, rangeEnd, CharacterStyle.class);

            if (DEBUG) {
                Log.d(LOGTAG, " found " + styleSpans.length + " spans @ " +
                              rangeStart + "-" + rangeEnd);
            }

            if (styleSpans.length == 0) {
                rangeType = (selStart == rangeStart && selEnd == rangeEnd)
                            ? IME_RANGE_SELECTEDRAWTEXT
                            : IME_RANGE_RAWINPUT;
            } else {
                rangeType = (selStart == rangeStart && selEnd == rangeEnd)
                            ? IME_RANGE_SELECTEDCONVERTEDTEXT
                            : IME_RANGE_CONVERTEDTEXT;
                tp.set(emptyTp);
                for (CharacterStyle span : styleSpans) {
                    span.updateDrawState(tp);
                }
                int tpUnderlineColor = 0;
                float tpUnderlineThickness = 0.0f;

                // These TextPaint fields only exist on Android ICS+ and are not in the SDK.
                if (Versions.feature14Plus) {
                    tpUnderlineColor = (Integer)getField(tp, "underlineColor", 0);
                    tpUnderlineThickness = (Float)getField(tp, "underlineThickness", 0.0f);
                }
                if (tpUnderlineColor != 0) {
                    rangeStyles |= IME_RANGE_UNDERLINE | IME_RANGE_LINECOLOR;
                    rangeLineColor = tpUnderlineColor;
                    // Approximately translate underline thickness to what Gecko understands
                    if (tpUnderlineThickness <= 0.5f) {
                        rangeLineStyle = IME_RANGE_LINE_DOTTED;
                    } else {
                        rangeLineStyle = IME_RANGE_LINE_SOLID;
                        if (tpUnderlineThickness >= 2.0f) {
                            rangeBoldLine = true;
                        }
                    }
                } else if (tp.isUnderlineText()) {
                    rangeStyles |= IME_RANGE_UNDERLINE;
                    rangeLineStyle = IME_RANGE_LINE_SOLID;
                }
                if (tp.getColor() != 0) {
                    rangeStyles |= IME_RANGE_FORECOLOR;
                    rangeForeColor = tp.getColor();
                }
                if (tp.bgColor != 0) {
                    rangeStyles |= IME_RANGE_BACKCOLOR;
                    rangeBackColor = tp.bgColor;
                }
            }
            onImeAddCompositionRange(
                    rangeStart - composingStart, rangeEnd - composingStart,
                    rangeType, rangeStyles, rangeLineStyle, rangeBoldLine,
                    rangeForeColor, rangeBackColor, rangeLineColor);
            rangeStart = rangeEnd;

            if (DEBUG) {
                Log.d(LOGTAG, " added " + rangeType +
                              " : " + Integer.toHexString(rangeStyles) +
                              " : " + Integer.toHexString(rangeForeColor) +
                              " : " + Integer.toHexString(rangeBackColor));
            }
        } while (rangeStart < composingEnd);
    }

    // GeckoEditableClient interface

    @Override
    public void sendKeyEvent(final KeyEvent event, int action, int metaState) {
        if (DEBUG) {
            assertOnIcThread();
            Log.d(LOGTAG, "sendKeyEvent(" + event + ", " + action + ", " + metaState + ")");
        }
        /*
           We are actually sending two events to Gecko here,
           1. Event from the event parameter (key event)
           2. Sync event from the mActionQueue.offer call
           The first event is a normal event that does not reply back to us,
           the second sync event will have a reply, during which we see that there is a pending
           event-type action, and update the selection/composition/etc. accordingly.
        */
        if (mNeedCompositionUpdate) {
            // Make sure Gecko selection is in-sync with Java selection first.
            icUpdateComposition();
        }
        onKeyEvent(event, action, metaState, /* isSynthesizedImeKey */ false);
        mActionQueue.offer(new Action(Action.TYPE_EVENT));
    }

    @Override
    public Editable getEditable() {
        if (!onIcThread()) {
            // Android may be holding an old InputConnection; ignore
            if (DEBUG) {
                Log.i(LOGTAG, "getEditable() called on non-IC thread");
            }
            return null;
        }
        if (mListener == null) {
            // We haven't initialized or we've been destroyed.
            return null;
        }
        return mProxy;
    }

    @Override
    public void setBatchMode(boolean inBatchMode) {
        if (!onIcThread()) {
            // Android may be holding an old InputConnection; ignore
            if (DEBUG) {
                Log.i(LOGTAG, "setBatchMode() called on non-IC thread");
            }
            return;
        }
        if (!inBatchMode && mNeedCompositionUpdate) {
            icUpdateComposition();
        }
        mInBatchMode = inBatchMode;
    }

    private void icUpdateComposition() {
        if (DEBUG) {
            assertOnIcThread();
        }
        mNeedCompositionUpdate = false;
        mActionQueue.syncWithGecko();
        icMaybeSendComposition(mText, /* useEntireText */ false, /* notifyGecko */ true);
    }

    private void geckoScheduleCompositionUpdate() {
        if (DEBUG) {
            ThreadUtils.assertOnGeckoThread();
        }
        // May be called from either Gecko or IC thread
        geckoPostToIc(new Runnable() {
            @Override
            public void run() {
                if (mInBatchMode || !mNeedCompositionUpdate) {
                    return;
                }
                if (!mActionQueue.isEmpty()) {
                    // We are likely to block here, so wait a little and try again.
                    mIcPostHandler.postDelayed(this, 10);
                    return;
                }
                icUpdateComposition();
            }
        });
    }

    @Override
    public void setSuppressKeyUp(boolean suppress) {
        if (DEBUG) {
            assertOnIcThread();
        }
        // Suppress key up event generated as a result of
        // translating characters to key events
        mSuppressKeyUp = suppress;
    }

    @Override
    public Handler setInputConnectionHandler(Handler handler) {
        if (handler == mIcPostHandler || !mFocused) {
            return mIcPostHandler;
        }
        if (DEBUG) {
            assertOnIcThread();
        }
        // There are three threads at this point: Gecko thread, old IC thread, and new IC
        // thread, and we want to safely switch from old IC thread to new IC thread.
        // We first send a TYPE_SET_HANDLER action to the Gecko thread; this ensures that
        // the Gecko thread is stopped at a known point. At the same time, the old IC
        // thread blocks on the action; this ensures that the old IC thread is stopped at
        // a known point. Finally, inside the Gecko thread, we post a Runnable to the old
        // IC thread; this Runnable switches from old IC thread to new IC thread. We
        // switch IC thread on the old IC thread to ensure any pending Runnables on the
        // old IC thread are processed before we switch over. Inside the Gecko thread, we
        // also post a Runnable to the new IC thread; this Runnable blocks until the
        // switch is complete; this ensures that the new IC thread won't accept
        // InputConnection calls until after the switch.
        mActionQueue.offer(Action.newSetHandler(handler));
        mActionQueue.syncWithGecko();
        return handler;
    }

    @Override // GeckoEditableClient
    public void postToInputConnection(final Runnable runnable) {
        mIcPostHandler.post(runnable);
    }

    @Override // GeckoEditableClient
    public void requestCursorUpdates(int requestMode) {
        onImeRequestCursorUpdates(requestMode);
    }

    private void geckoSetIcHandler(final Handler newHandler) {
        geckoPostToIc(new Runnable() { // posting to old IC thread
            @Override
            public void run() {
                synchronized (newHandler) {
                    mIcRunHandler = newHandler;
                    newHandler.notify();
                }
            }
        });

        // At this point, all future Runnables should be posted to the new IC thread, but
        // we don't switch mIcRunHandler yet because there may be pending Runnables on the
        // old IC thread still waiting to run.
        mIcPostHandler = newHandler;

        geckoPostToIc(new Runnable() { // posting to new IC thread
            @Override
            public void run() {
                synchronized (newHandler) {
                    while (mIcRunHandler != newHandler) {
                        try {
                            newHandler.wait();
                        } catch (InterruptedException e) {
                        }
                    }
                }
            }
        });
    }

    // GeckoEditableListener interface

    private void geckoActionReply() {
        if (DEBUG) {
            // GeckoEditableListener methods should all be called from the Gecko thread
            ThreadUtils.assertOnGeckoThread();
        }

        final Action action = mActionQueue.peek();
        if (action == null) {
            throw new IllegalStateException("empty actions queue");
        }

        if (DEBUG) {
            Log.d(LOGTAG, "reply: Action(" +
                          getConstantName(Action.class, "TYPE_", action.mType) + ")");
        }
        switch (action.mType) {
        case Action.TYPE_SET_SPAN:
            mText.setSpan(action.mSpanObject, action.mStart, action.mEnd, action.mSpanFlags);
            break;

        case Action.TYPE_REMOVE_SPAN:
            mText.removeSpan(action.mSpanObject);
            break;

        case Action.TYPE_SET_HANDLER:
            geckoSetIcHandler(action.mHandler);
            break;
        }

        if (action.mUpdateComposition) {
            geckoScheduleCompositionUpdate();
        }
    }

    private void notifyCommitComposition() {
        // Gecko already committed its composition. However, Android keyboards
        // have trouble dealing with us removing the composition manually on
        // the Java side. Therefore, we keep the composition intact on the Java
        // side. The text content should still be in-sync on both sides.
    }

    private void notifyCancelComposition() {
        // Composition should have been canceled on our side
        // through text update notifications; verify that here.
        if (DEBUG) {
            final Object[] spans = mText.getSpans(0, mText.length(), Object.class);
            for (Object span : spans) {
                if ((mText.getSpanFlags(span) & Spanned.SPAN_COMPOSING) != 0) {
                    throw new IllegalStateException("composition not cancelled");
                }
            }
        }
    }

    @WrapForJNI(calledFrom = "gecko") @Override
    public void notifyIME(final int type) {
        if (DEBUG) {
            // GeckoEditableListener methods should all be called from the Gecko thread
            ThreadUtils.assertOnGeckoThread();
            // NOTIFY_IME_REPLY_EVENT is logged separately, inside geckoActionReply()
            if (type != NOTIFY_IME_REPLY_EVENT) {
                Log.d(LOGTAG, "notifyIME(" +
                              getConstantName(GeckoEditableListener.class, "NOTIFY_IME_", type) +
                              ")");
            }
        }

        if (type == NOTIFY_IME_REPLY_EVENT) {
            try {
                if (mGeckoFocused) {
                    // When mGeckoFocused is false, the reply is for a stale action,
                    // and we should not do anything
                    geckoActionReply();
                } else if (DEBUG) {
                    Log.d(LOGTAG, "discarding stale reply");
                }
            } finally {
                // Ensure action is always removed from queue
                // even if stale action results in exception in geckoActionReply
                mActionQueue.poll();
            }
            return;
        } else if (type == NOTIFY_IME_TO_COMMIT_COMPOSITION) {
            notifyCommitComposition();
            return;
        } else if (type == NOTIFY_IME_TO_CANCEL_COMPOSITION) {
            notifyCancelComposition();
            return;
        }

        geckoPostToIc(new Runnable() {
            @Override
            public void run() {
                if (type == NOTIFY_IME_OF_FOCUS) {
                    mFocused = true;
                    // Unmask events on the Gecko side
                    mActionQueue.offer(new Action(Action.TYPE_ACKNOWLEDGE_FOCUS));
                }

                // Make sure there are no other things going on. If we sent
                // Action.TYPE_ACKNOWLEDGE_FOCUS, this line also makes us
                // wait for Gecko to update us on the newly focused content
                mActionQueue.syncWithGecko();
                if (mListener != null) {
                    mListener.notifyIME(type);
                }

                // Unset mFocused after we call syncWithGecko because
                // syncWithGecko becomes a no-op when mFocused is false.
                if (type == NOTIFY_IME_OF_BLUR) {
                    mFocused = false;
                }
            }
        });

        // Register/unregister Gecko-side text selection listeners
        // and update the mGeckoFocused flag.
        if (type == NOTIFY_IME_OF_BLUR && mGeckoFocused) {
            // Check for focus here because Gecko may send us a blur before a focus in some
            // cases, and we don't want to unregister an event that was not registered.
            mGeckoFocused = false;
            mSuppressCompositions = false;
            EventDispatcher.getInstance().
                unregisterGeckoThreadListener(this, "TextSelection:DraggingHandle");
        } else if (type == NOTIFY_IME_OF_FOCUS) {
            mGeckoFocused = true;
            mSuppressCompositions = false;
            EventDispatcher.getInstance().
                registerGeckoThreadListener(this, "TextSelection:DraggingHandle");
        }
    }

    @WrapForJNI(calledFrom = "gecko") @Override
    public void notifyIMEContext(final int state, final String typeHint,
                                 final String modeHint, final String actionHint) {
        if (DEBUG) {
            // GeckoEditableListener methods should all be called from the Gecko thread
            ThreadUtils.assertOnGeckoThread();
            Log.d(LOGTAG, "notifyIMEContext(" +
                          getConstantName(GeckoEditableListener.class, "IME_STATE_", state) +
                          ", \"" + typeHint + "\", \"" + modeHint + "\", \"" + actionHint + "\")");
        }
        geckoPostToIc(new Runnable() {
            @Override
            public void run() {
                if (mListener == null) {
                    return;
                }
                mListener.notifyIMEContext(state, typeHint, modeHint, actionHint);
            }
        });
    }

    @WrapForJNI(calledFrom = "gecko") @Override
    public void onSelectionChange(int start, int end) {
        if (DEBUG) {
            // GeckoEditableListener methods should all be called from the Gecko thread
            ThreadUtils.assertOnGeckoThread();
            Log.d(LOGTAG, "onSelectionChange(" + start + ", " + end + ")");
        }
        if (start < 0 || start > mText.length() || end < 0 || end > mText.length()) {
            Log.e(LOGTAG, "invalid selection notification range: " +
                  start + " to " + end + ", length: " + mText.length());
            throw new IllegalArgumentException("invalid selection notification range");
        }

        if (mIgnoreSelectionChange) {
            start = Selection.getSelectionStart(mText);
            end = Selection.getSelectionEnd(mText);
            mIgnoreSelectionChange = false;

        } else {
            Selection.setSelection(mText, start, end);
        }

        final int newStart = start;
        final int newEnd = end;
        geckoPostToIc(new Runnable() {
            @Override
            public void run() {
                if (mListener == null) {
                    return;
                }
                mListener.onSelectionChange(newStart, newEnd);
            }
        });
    }

    private void geckoReplaceText(int start, int oldEnd, CharSequence newText) {
        mText.replace(start, oldEnd, newText);
    }

    private boolean geckoIsSameText(int start, int oldEnd, CharSequence newText) {
        return oldEnd - start == newText.length() &&
               TextUtils.regionMatches(mText, start, newText, 0, oldEnd - start);
    }

    @WrapForJNI(calledFrom = "gecko") @Override
    public void onTextChange(final CharSequence text, final int start,
                             final int unboundedOldEnd, final int unboundedNewEnd) {
        if (DEBUG) {
            // GeckoEditableListener methods should all be called from the Gecko thread
            ThreadUtils.assertOnGeckoThread();
            StringBuilder sb = new StringBuilder("onTextChange(");
            debugAppend(sb, text);
            sb.append(", ").append(start).append(", ")
                .append(unboundedOldEnd).append(", ")
                .append(unboundedNewEnd).append(")");
            Log.d(LOGTAG, sb.toString());
        }
        if (start < 0 || start > unboundedOldEnd) {
            Log.e(LOGTAG, "invalid text notification range: " +
                  start + " to " + unboundedOldEnd);
            throw new IllegalArgumentException("invalid text notification range");
        }
        /* For the "end" parameters, Gecko can pass in a large
           number to denote "end of the text". Fix that here */
        final int oldEnd = unboundedOldEnd > mText.length() ? mText.length() : unboundedOldEnd;
        // new end should always match text
        if (unboundedOldEnd <= mText.length() &&
                unboundedNewEnd != (start + text.length())) {
            Log.e(LOGTAG, "newEnd does not match text: " + unboundedNewEnd + " vs " +
                  (start + text.length()));
            throw new IllegalArgumentException("newEnd does not match text");
        }
        final int newEnd = start + text.length();
        final Action action = mActionQueue.peek();

        if (action != null && action.mType == Action.TYPE_ACKNOWLEDGE_FOCUS) {
            // Simply replace the text for newly-focused editors.
            mText.replace(0, mText.length(), text);

        } else if (action != null &&
                action.mType == Action.TYPE_REPLACE_TEXT &&
                start <= action.mStart &&
                oldEnd >= action.mEnd &&
                newEnd >= action.mStart + action.mSequence.length()) {

            // Try to preserve both old spans and new spans in action.mSequence.
            // indexInText is where we can find waction.mSequence within the passed in text.
            final int startWithinText = action.mStart - start;
            int indexInText = TextUtils.indexOf(text, action.mSequence, startWithinText);
            if (indexInText < 0 && startWithinText >= action.mSequence.length()) {
                indexInText = text.toString().lastIndexOf(action.mSequence.toString(),
                                                          startWithinText);
            }

            if (indexInText < 0) {
                // Text was changed from under us. We are forced to discard any new spans.
                geckoReplaceText(start, oldEnd, text);

                // Don't ignore the next selection change because we are forced to re-sync
                // with Gecko here.
                mIgnoreSelectionChange = false;

            } else if (indexInText == 0 && text.length() == action.mSequence.length()) {
                // The new text exactly matches our sequence, so do a direct replace.
                geckoReplaceText(start, oldEnd, action.mSequence);

                // Ignore the next selection change because the selection change is a
                // side-effect of the replace-text event we sent.
                mIgnoreSelectionChange = true;

            } else {
                // The sequence is embedded within the changed text, so we have to perform
                // replacement in parts. First replace part of text before the sequence.
                geckoReplaceText(start, action.mStart, text.subSequence(0, indexInText));

                // Then Replace the sequence itself to preserve new spans.
                final int actionStart = indexInText + start;
                geckoReplaceText(actionStart, actionStart + action.mEnd - action.mStart,
                                 action.mSequence);

                // Finally replace part of text after the sequence.
                final int actionEnd = actionStart + action.mSequence.length();
                geckoReplaceText(actionEnd, actionEnd + oldEnd - action.mEnd,
                                 text.subSequence(actionEnd - start, text.length()));
            }

        } else if (geckoIsSameText(start, oldEnd, text)) {
            // Nothing to do because the text is the same. This could happen when
            // the composition is updated for example, in which case we want to keep the
            // Java selection.
            mIgnoreSelectionChange = mIgnoreSelectionChange ||
                    (action != null && action.mType == Action.TYPE_UPDATE_COMPOSITION);
            return;

        } else {
            // Gecko side initiated the text change.
            geckoReplaceText(start, oldEnd, text);
        }

        geckoPostToIc(new Runnable() {
            @Override
            public void run() {
                if (mListener == null) {
                    return;
                }
                mListener.onTextChange(text, start, oldEnd, newEnd);
            }
        });
    }

    @WrapForJNI(calledFrom = "gecko") @Override
    public void onDefaultKeyEvent(final KeyEvent event) {
        if (DEBUG) {
            // GeckoEditableListener methods should all be called from the Gecko thread
            ThreadUtils.assertOnGeckoThread();
            StringBuilder sb = new StringBuilder("onDefaultKeyEvent(");
            sb.append("action=").append(event.getAction()).append(", ")
                .append("keyCode=").append(event.getKeyCode()).append(", ")
                .append("metaState=").append(event.getMetaState()).append(", ")
                .append("time=").append(event.getEventTime()).append(", ")
                .append("repeatCount=").append(event.getRepeatCount()).append(")");
            Log.d(LOGTAG, sb.toString());
        }

        geckoPostToIc(new Runnable() {
            @Override
            public void run() {
                if (mListener == null) {
                    return;
                }
                mListener.onDefaultKeyEvent(event);
            }
        });
    }

    @WrapForJNI(calledFrom = "gecko") @Override
    public void updateCompositionRects(final RectF[] aRects) {
        if (DEBUG) {
            // GeckoEditableListener methods should all be called from the Gecko thread
            ThreadUtils.assertOnGeckoThread();
            Log.d(LOGTAG, "updateCompositionRects(aRects.length = " + aRects.length + ")");
        }
        geckoPostToIc(new Runnable() {
            @Override
            public void run() {
                if (mListener == null) {
                    return;
                }
                mListener.updateCompositionRects(aRects);
            }
        });
    }

    // InvocationHandler interface

    static String getConstantName(Class<?> cls, String prefix, Object value) {
        for (Field fld : cls.getDeclaredFields()) {
            try {
                if (fld.getName().startsWith(prefix) &&
                    fld.get(null).equals(value)) {
                    return fld.getName();
                }
            } catch (IllegalAccessException e) {
            }
        }
        return String.valueOf(value);
    }

    static StringBuilder debugAppend(StringBuilder sb, Object obj) {
        if (obj == null) {
            sb.append("null");
        } else if (obj instanceof GeckoEditable) {
            sb.append("GeckoEditable");
        } else if (Proxy.isProxyClass(obj.getClass())) {
            debugAppend(sb, Proxy.getInvocationHandler(obj));
        } else if (obj instanceof CharSequence) {
            sb.append('"').append(obj.toString().replace('\n', '\u21b2')).append('"');
        } else if (obj.getClass().isArray()) {
            sb.append(obj.getClass().getComponentType().getSimpleName()).append('[')
              .append(Array.getLength(obj)).append(']');
        } else {
            sb.append(obj);
        }
        return sb;
    }

    @Override
    public Object invoke(Object proxy, Method method, Object[] args)
                         throws Throwable {
        Object target;
        final Class<?> methodInterface = method.getDeclaringClass();
        if (DEBUG) {
            // Editable methods should all be called from the IC thread
            assertOnIcThread();
        }
        if (methodInterface == Editable.class ||
                methodInterface == Appendable.class ||
                methodInterface == Spannable.class) {
            // Method alters the Editable; route calls to our implementation
            target = this;
        } else {
            // Method queries the Editable; must sync with Gecko first
            // then call on the inner Editable itself
            mActionQueue.syncWithGecko();
            target = mText;
        }
        Object ret;
        try {
            ret = method.invoke(target, args);
        } catch (InvocationTargetException e) {
            // Bug 817386
            // Most likely Gecko has changed the text while GeckoInputConnection is
            // trying to access the text. If we pass through the exception here, Fennec
            // will crash due to a lack of exception handler. Log the exception and
            // return an empty value instead.
            if (!(e.getCause() instanceof IndexOutOfBoundsException)) {
                // Only handle IndexOutOfBoundsException for now,
                // as other exceptions might signal other bugs
                throw e;
            }
            Log.w(LOGTAG, "Exception in GeckoEditable." + method.getName(), e.getCause());
            Class<?> retClass = method.getReturnType();
            if (retClass == Character.TYPE) {
                ret = '\0';
            } else if (retClass == Integer.TYPE) {
                ret = 0;
            } else if (retClass == String.class) {
                ret = "";
            } else {
                ret = null;
            }
        }
        if (DEBUG) {
            StringBuilder log = new StringBuilder(method.getName());
            log.append("(");
            if (args != null) {
                for (Object arg : args) {
                    debugAppend(log, arg).append(", ");
                }
                if (args.length > 0) {
                    log.setLength(log.length() - 2);
                }
            }
            if (method.getReturnType().equals(Void.TYPE)) {
                log.append(")");
            } else {
                debugAppend(log.append(") = "), ret);
            }
            Log.d(LOGTAG, log.toString());
        }
        return ret;
    }

    // Spannable interface

    @Override
    public void removeSpan(Object what) {
        if (what == Selection.SELECTION_START ||
                what == Selection.SELECTION_END) {
            Log.w(LOGTAG, "selection removed with removeSpan()");
        }

        // "what" could be a composing span (BaseInputConnection.COMPOSING extends
        // NoCopySpan), so we should update the Gecko composition
        final boolean update = !mNeedCompositionUpdate && what instanceof NoCopySpan;
        mActionQueue.offer(Action.newRemoveSpan(what, update));
        mNeedCompositionUpdate |= update;
    }

    @Override
    public void setSpan(Object what, int start, int end, int flags) {
        // If mUpdateComposition is true, it means something in the queue will be
        // scheduling an update, so it would be redundant to have this action schedule
        // another update.
        final boolean update = !mNeedCompositionUpdate &&
                (flags & Spanned.SPAN_INTERMEDIATE) == 0 && (
                (flags & Spanned.SPAN_COMPOSING) != 0 ||
                what == Selection.SELECTION_START ||
                what == Selection.SELECTION_END);
        mActionQueue.offer(Action.newSetSpan(what, start, end, flags, update));
        mNeedCompositionUpdate |= update;
    }

    // Appendable interface

    @Override
    public Editable append(CharSequence text) {
        return replace(mProxy.length(), mProxy.length(), text, 0, text.length());
    }

    @Override
    public Editable append(CharSequence text, int start, int end) {
        return replace(mProxy.length(), mProxy.length(), text, start, end);
    }

    @Override
    public Editable append(char text) {
        return replace(mProxy.length(), mProxy.length(), String.valueOf(text), 0, 1);
    }

    // Editable interface

    @Override
    public InputFilter[] getFilters() {
        return mFilters;
    }

    @Override
    public void setFilters(InputFilter[] filters) {
        mFilters = filters;
    }

    @Override
    public void clearSpans() {
        /* XXX this clears the selection spans too,
           but there is no way to clear the corresponding selection in Gecko */
        Log.w(LOGTAG, "selection cleared with clearSpans()");
        mText.clearSpans();
    }

    @Override
    public Editable replace(int st, int en,
            CharSequence source, int start, int end) {

        CharSequence text = source;
        if (start < 0 || start > end || end > text.length()) {
            Log.e(LOGTAG, "invalid replace offsets: " +
                  start + " to " + end + ", length: " + text.length());
            throw new IllegalArgumentException("invalid replace offsets");
        }
        if (start != 0 || end != text.length()) {
            text = text.subSequence(start, end);
        }
        if (mFilters != null) {
            // Filter text before sending the request to Gecko
            for (int i = 0; i < mFilters.length; ++i) {
                final CharSequence cs = mFilters[i].filter(
                        text, 0, text.length(), mProxy, st, en);
                if (cs != null) {
                    text = cs;
                }
            }
        }
        if (text == source) {
            // Always create a copy
            text = new SpannableString(source);
        }
        mActionQueue.offer(Action.newReplaceText(text,
                Math.min(st, en), Math.max(st, en)));
        return mProxy;
    }

    @Override
    public void clear() {
        replace(0, mProxy.length(), "", 0, 0);
    }

    @Override
    public Editable delete(int st, int en) {
        return replace(st, en, "", 0, 0);
    }

    @Override
    public Editable insert(int where, CharSequence text,
                                int start, int end) {
        return replace(where, where, text, start, end);
    }

    @Override
    public Editable insert(int where, CharSequence text) {
        return replace(where, where, text, 0, text.length());
    }

    @Override
    public Editable replace(int st, int en, CharSequence text) {
        return replace(st, en, text, 0, text.length());
    }

    /* GetChars interface */

    @Override
    public void getChars(int start, int end, char[] dest, int destoff) {
        /* overridden Editable interface methods in GeckoEditable must not be called directly
           outside of GeckoEditable. Instead, the call must go through mProxy, which ensures
           that Java is properly synchronized with Gecko */
        throw new UnsupportedOperationException("method must be called through mProxy");
    }

    /* Spanned interface */

    @Override
    public int getSpanEnd(Object tag) {
        throw new UnsupportedOperationException("method must be called through mProxy");
    }

    @Override
    public int getSpanFlags(Object tag) {
        throw new UnsupportedOperationException("method must be called through mProxy");
    }

    @Override
    public int getSpanStart(Object tag) {
        throw new UnsupportedOperationException("method must be called through mProxy");
    }

    @Override
    public <T> T[] getSpans(int start, int end, Class<T> type) {
        throw new UnsupportedOperationException("method must be called through mProxy");
    }

    @Override
    @SuppressWarnings("rawtypes") // nextSpanTransition uses raw Class in its Android declaration
    public int nextSpanTransition(int start, int limit, Class type) {
        throw new UnsupportedOperationException("method must be called through mProxy");
    }

    /* CharSequence interface */

    @Override
    public char charAt(int index) {
        throw new UnsupportedOperationException("method must be called through mProxy");
    }

    @Override
    public int length() {
        throw new UnsupportedOperationException("method must be called through mProxy");
    }

    @Override
    public CharSequence subSequence(int start, int end) {
        throw new UnsupportedOperationException("method must be called through mProxy");
    }

    @Override
    public String toString() {
        throw new UnsupportedOperationException("method must be called through mProxy");
    }

    // GeckoEventListener implementation

    @Override
    public void handleMessage(String event, JSONObject message) {
        if (!"TextSelection:DraggingHandle".equals(event)) {
            return;
        }

        mSuppressCompositions = message.optBoolean("dragging", false);
    }
}

