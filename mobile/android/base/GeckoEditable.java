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
import org.mozilla.gecko.gfx.InputConnectionHandler;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.ThreadUtils.AssertBehavior;

import android.os.Handler;
import android.os.Looper;
import android.text.Editable;
import android.text.InputFilter;
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
final class GeckoEditable
        implements InvocationHandler, Editable,
                   GeckoEditableClient, GeckoEditableListener, GeckoEventListener {

    private static final boolean DEBUG = false;
    private static final String LOGTAG = "GeckoEditable";

    // Filters to implement Editable's filtering functionality
    private InputFilter[] mFilters;

    private final SpannableStringBuilder mText;
    private final SpannableStringBuilder mChangedText;
    private final Editable mProxy;
    private final ActionQueue mActionQueue;

    // mIcRunHandler is the Handler that currently runs Gecko-to-IC Runnables
    // mIcPostHandler is the Handler to post Gecko-to-IC Runnables to
    // The two can be different when switching from one handler to another
    private Handler mIcRunHandler;
    private Handler mIcPostHandler;

    private GeckoEditableListener mListener;
    private int mSavedSelectionStart;
    private volatile int mGeckoUpdateSeqno;
    private int mIcUpdateSeqno;
    private int mLastIcUpdateSeqno;
    private boolean mUpdateGecko;
    private boolean mFocused; // Used by IC thread
    private boolean mGeckoFocused; // Used by Gecko thread
    private volatile boolean mSuppressCompositions;
    private volatile boolean mSuppressKeyUp;

    /* An action that alters the Editable

       Each action corresponds to a Gecko event. While the Gecko event is being sent to the Gecko
       thread, the action stays on top of mActions queue. After the Gecko event is processed and
       replied, the action is removed from the queue
    */
    private static final class Action {
        // For input events (keypress, etc.); use with IME_SYNCHRONIZE
        static final int TYPE_EVENT = 0;
        // For Editable.replace() call; use with IME_REPLACE_TEXT
        static final int TYPE_REPLACE_TEXT = 1;
        /* For Editable.setSpan(Selection...) call; use with IME_SYNCHRONIZE
           Note that we don't use this with IME_SET_SELECTION because we don't want to update the
           Gecko selection at the point of this action. The Gecko selection is updated only after
           IC has updated its selection (during IME_SYNCHRONIZE reply) */
        static final int TYPE_SET_SELECTION = 2;
        // For Editable.setSpan() call; use with IME_SYNCHRONIZE
        static final int TYPE_SET_SPAN = 3;
        // For Editable.removeSpan() call; use with IME_SYNCHRONIZE
        static final int TYPE_REMOVE_SPAN = 4;
        // For focus events (in notifyIME); use with IME_ACKNOWLEDGE_FOCUS
        static final int TYPE_ACKNOWLEDGE_FOCUS = 5;
        // For switching handler; use with IME_SYNCHRONIZE
        static final int TYPE_SET_HANDLER = 6;
        // For Editable.replace() call involving compositions; use with IME_COMPOSE_TEXT
        static final int TYPE_COMPOSE_TEXT = 7;

        final int mType;
        int mStart;
        int mEnd;
        CharSequence mSequence;
        Object mSpanObject;
        int mSpanFlags;
        boolean mShouldUpdate;
        Handler mHandler;

        Action(int type) {
            mType = type;
        }

        static Action newReplaceText(CharSequence text, int start, int end) {
            if (start < 0 || start > end) {
                Log.e(LOGTAG, "invalid replace text offsets: " + start + " to " + end);
                throw new IllegalArgumentException("invalid replace text offsets");
            }

            int actionType = TYPE_REPLACE_TEXT;

            if (text instanceof Spanned) {
                final Spanned spanned = (Spanned) text;
                final Object[] spans = spanned.getSpans(0, spanned.length(), Object.class);

                for (Object span : spans) {
                    if ((spanned.getSpanFlags(span) & Spanned.SPAN_COMPOSING) != 0) {
                        actionType = TYPE_COMPOSE_TEXT;
                        break;
                    }
                }
            }

            final Action action = new Action(actionType);
            action.mSequence = text;
            action.mStart = start;
            action.mEnd = end;
            return action;
        }

        static Action newSetSelection(int start, int end) {
            // start == -1 when the start offset should remain the same
            // end == -1 when the end offset should remain the same
            if (start < -1 || end < -1) {
                Log.e(LOGTAG, "invalid selection offsets: " + start + " to " + end);
                throw new IllegalArgumentException("invalid selection offsets");
            }
            final Action action = new Action(TYPE_SET_SELECTION);
            action.mStart = start;
            action.mEnd = end;
            return action;
        }

        static Action newSetSpan(Object object, int start, int end, int flags) {
            if (start < 0 || start > end) {
                Log.e(LOGTAG, "invalid span offsets: " + start + " to " + end);
                throw new IllegalArgumentException("invalid span offsets");
            }
            final Action action = new Action(TYPE_SET_SPAN);
            action.mSpanObject = object;
            action.mStart = start;
            action.mEnd = end;
            action.mSpanFlags = flags;
            return action;
        }

        static Action newRemoveSpan(Object object) {
            final Action action = new Action(TYPE_REMOVE_SPAN);
            action.mSpanObject = object;
            return action;
        }

        static Action newSetHandler(Handler handler) {
            final Action action = new Action(TYPE_SET_HANDLER);
            action.mHandler = handler;
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
            /* Events don't need update because they generate text/selection
               notifications which will do the updating for us */
            if (action.mType != Action.TYPE_EVENT &&
                action.mType != Action.TYPE_ACKNOWLEDGE_FOCUS &&
                action.mType != Action.TYPE_SET_HANDLER) {
                action.mShouldUpdate = mUpdateGecko;
            }
            if (mActions.isEmpty()) {
                mActionsActive.acquireUninterruptibly();
                mActions.offer(action);
            } else synchronized(this) {
                // tryAcquire here in case Gecko thread has just released it
                mActionsActive.tryAcquire();
                mActions.offer(action);
            }

            switch (action.mType) {
            case Action.TYPE_EVENT:
            case Action.TYPE_SET_SELECTION:
            case Action.TYPE_SET_SPAN:
            case Action.TYPE_REMOVE_SPAN:
            case Action.TYPE_SET_HANDLER:
                GeckoAppShell.sendEventToGecko(GeckoEvent.createIMEEvent(
                        GeckoEvent.ImeAction.IME_SYNCHRONIZE));
                break;

            case Action.TYPE_COMPOSE_TEXT:
                // Send different event for composing text.
                GeckoAppShell.sendEventToGecko(GeckoEvent.createIMEComposeEvent(
                        action.mStart, action.mEnd, action.mSequence.toString()));
                return;

            case Action.TYPE_REPLACE_TEXT:
                // try key events first
                sendCharKeyEvents(action);
                GeckoAppShell.sendEventToGecko(GeckoEvent.createIMEReplaceEvent(
                        action.mStart, action.mEnd, action.mSequence.toString()));
                break;

            case Action.TYPE_ACKNOWLEDGE_FOCUS:
                GeckoAppShell.sendEventToGecko(GeckoEvent.createIMEEvent(
                        GeckoEvent.ImeAction.IME_ACKNOWLEDGE_FOCUS));
                break;

            default:
                throw new IllegalStateException("Action not processed");
            }

            ++mIcUpdateSeqno;
        }

        private KeyEvent [] synthesizeKeyEvents(CharSequence cs) {
            try {
                if (mKeyMap == null) {
                    mKeyMap = KeyCharacterMap.load(
                        Versions.preHC ? KeyCharacterMap.ALPHA :
                                         KeyCharacterMap.VIRTUAL_KEYBOARD);
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
            if (action.mSequence.length() == 0 ||
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
                GeckoAppShell.sendEventToGecko(GeckoEvent.createIMEKeyEvent(event));
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

            synchronized(this) {
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

    GeckoEditable() {
        mActionQueue = new ActionQueue();
        mSavedSelectionStart = -1;
        mUpdateGecko = true;

        mText = new SpannableStringBuilder();
        mChangedText = new SpannableStringBuilder();

        final Class<?>[] PROXY_INTERFACES = { Editable.class };
        mProxy = (Editable)Proxy.newProxyInstance(
                Editable.class.getClassLoader(),
                PROXY_INTERFACES, this);

        LayerView v = GeckoAppShell.getLayerView();
        mListener = GeckoInputConnection.create(v, this);

        mIcRunHandler = mIcPostHandler = ThreadUtils.getUiHandler();
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

    private void geckoUpdateGecko(final boolean force) {
        /* We do not increment the seqno here, but only check it, because geckoUpdateGecko is a
           request for update. If we incremented the seqno here, geckoUpdateGecko would have
           prevented other updates from occurring */
        final int seqnoWhenPosted = mGeckoUpdateSeqno;

        geckoPostToIc(new Runnable() {
            @Override
            public void run() {
                mActionQueue.syncWithGecko();
                if (seqnoWhenPosted == mGeckoUpdateSeqno) {
                    icUpdateGecko(force);
                }
            }
        });
    }

    private Object getField(Object obj, String field, Object def) {
        try {
            return obj.getClass().getField(field).get(obj);
        } catch (Exception e) {
            return def;
        }
    }

    private void icUpdateGecko(boolean force) {

        // Skip if receiving a repeated request, or
        // if suppressing compositions during text selection.
        if ((!force && mIcUpdateSeqno == mLastIcUpdateSeqno) ||
            mSuppressCompositions) {
            if (DEBUG) {
                Log.d(LOGTAG, "icUpdateGecko() skipped");
            }
            return;
        }
        mLastIcUpdateSeqno = mIcUpdateSeqno;
        mActionQueue.syncWithGecko();

        if (DEBUG) {
            Log.d(LOGTAG, "icUpdateGecko()");
        }

        final int selStart = mText.getSpanStart(Selection.SELECTION_START);
        final int selEnd = mText.getSpanEnd(Selection.SELECTION_END);
        int composingStart = mText.length();
        int composingEnd = 0;
        Object[] spans = mText.getSpans(0, composingStart, Object.class);

        for (Object span : spans) {
            if ((mText.getSpanFlags(span) & Spanned.SPAN_COMPOSING) != 0) {
                composingStart = Math.min(composingStart, mText.getSpanStart(span));
                composingEnd = Math.max(composingEnd, mText.getSpanEnd(span));
            }
        }
        if (DEBUG) {
            Log.d(LOGTAG, " range = " + composingStart + "-" + composingEnd);
            Log.d(LOGTAG, " selection = " + selStart + "-" + selEnd);
        }
        if (composingStart >= composingEnd) {
            if (selStart >= 0 && selEnd >= 0) {
                GeckoAppShell.sendEventToGecko(
                        GeckoEvent.createIMESelectEvent(selStart, selEnd));
            } else {
                GeckoAppShell.sendEventToGecko(GeckoEvent.createIMEEvent(
                        GeckoEvent.ImeAction.IME_REMOVE_COMPOSITION));
            }
            return;
        }

        if (selEnd >= composingStart && selEnd <= composingEnd) {
            GeckoAppShell.sendEventToGecko(GeckoEvent.createIMERangeEvent(
                    selEnd - composingStart, selEnd - composingStart,
                    GeckoEvent.IME_RANGE_CARETPOSITION, 0, 0, false, 0, 0, 0));
        }
        int rangeStart = composingStart;
        TextPaint tp = new TextPaint();
        TextPaint emptyTp = new TextPaint();
        // set initial foreground color to 0, because we check for tp.getColor() == 0
        // below to decide whether to pass a foreground color to Gecko
        emptyTp.setColor(0);
        do {
            int rangeType, rangeStyles = 0, rangeLineStyle = GeckoEvent.IME_RANGE_LINE_NONE;
            boolean rangeBoldLine = false;
            int rangeForeColor = 0, rangeBackColor = 0, rangeLineColor = 0;
            int rangeEnd = mText.nextSpanTransition(rangeStart, composingEnd, Object.class);

            if (selStart > rangeStart && selStart < rangeEnd) {
                rangeEnd = selStart;
            } else if (selEnd > rangeStart && selEnd < rangeEnd) {
                rangeEnd = selEnd;
            }
            CharacterStyle[] styleSpans =
                    mText.getSpans(rangeStart, rangeEnd, CharacterStyle.class);

            if (DEBUG) {
                Log.d(LOGTAG, " found " + styleSpans.length + " spans @ " +
                              rangeStart + "-" + rangeEnd);
            }

            if (styleSpans.length == 0) {
                rangeType = (selStart == rangeStart && selEnd == rangeEnd)
                            ? GeckoEvent.IME_RANGE_SELECTEDRAWTEXT
                            : GeckoEvent.IME_RANGE_RAWINPUT;
            } else {
                rangeType = (selStart == rangeStart && selEnd == rangeEnd)
                            ? GeckoEvent.IME_RANGE_SELECTEDCONVERTEDTEXT
                            : GeckoEvent.IME_RANGE_CONVERTEDTEXT;
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
                    rangeStyles |= GeckoEvent.IME_RANGE_UNDERLINE | GeckoEvent.IME_RANGE_LINECOLOR;
                    rangeLineColor = tpUnderlineColor;
                    // Approximately translate underline thickness to what Gecko understands
                    if (tpUnderlineThickness <= 0.5f) {
                        rangeLineStyle = GeckoEvent.IME_RANGE_LINE_DOTTED;
                    } else {
                        rangeLineStyle = GeckoEvent.IME_RANGE_LINE_SOLID;
                        if (tpUnderlineThickness >= 2.0f) {
                            rangeBoldLine = true;
                        }
                    }
                } else if (tp.isUnderlineText()) {
                    rangeStyles |= GeckoEvent.IME_RANGE_UNDERLINE;
                    rangeLineStyle = GeckoEvent.IME_RANGE_LINE_SOLID;
                }
                if (tp.getColor() != 0) {
                    rangeStyles |= GeckoEvent.IME_RANGE_FORECOLOR;
                    rangeForeColor = tp.getColor();
                }
                if (tp.bgColor != 0) {
                    rangeStyles |= GeckoEvent.IME_RANGE_BACKCOLOR;
                    rangeBackColor = tp.bgColor;
                }
            }
            GeckoAppShell.sendEventToGecko(GeckoEvent.createIMERangeEvent(
                    rangeStart - composingStart, rangeEnd - composingStart,
                    rangeType, rangeStyles, rangeLineStyle, rangeBoldLine,
                    rangeForeColor, rangeBackColor, rangeLineColor));
            rangeStart = rangeEnd;

            if (DEBUG) {
                Log.d(LOGTAG, " added " + rangeType +
                              " : " + Integer.toHexString(rangeStyles) +
                              " : " + Integer.toHexString(rangeForeColor) +
                              " : " + Integer.toHexString(rangeBackColor));
            }
        } while (rangeStart < composingEnd);

        GeckoAppShell.sendEventToGecko(GeckoEvent.createIMECompositionEvent(
                composingStart, composingEnd));
    }

    // GeckoEditableClient interface

    @Override
    public void sendEvent(final GeckoEvent event) {
        if (DEBUG) {
            assertOnIcThread();
            Log.d(LOGTAG, "sendEvent(" + event + ")");
        }
        /*
           We are actually sending two events to Gecko here,
           1. Event from the event parameter (key event, etc.)
           2. Sync event from the mActionQueue.offer call
           The first event is a normal GeckoEvent that does not reply back to us,
           the second sync event will have a reply, during which we see that there is a pending
           event-type action, and update the selection/composition/etc. accordingly.
        */
        GeckoAppShell.sendEventToGecko(event);
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
        return mProxy;
    }

    @Override
    public void setUpdateGecko(boolean update) {
        if (!onIcThread()) {
            // Android may be holding an old InputConnection; ignore
            if (DEBUG) {
                Log.i(LOGTAG, "setUpdateGecko() called on non-IC thread");
            }
            return;
        }
        if (update) {
            icUpdateGecko(false);
        }
        mUpdateGecko = update;
    }

    @Override
    public void setSuppressKeyUp(boolean suppress) {
        if (DEBUG) {
            // only used by key event handler
            ThreadUtils.assertOnUiThread();
        }
        // Suppress key up event generated as a result of
        // translating characters to key events
        mSuppressKeyUp = suppress;
    }

    @Override
    public Handler getInputConnectionHandler() {
        // Can be called from either UI thread or IC thread;
        // care must be taken to avoid race conditions
        return mIcRunHandler;
    }

    @Override
    public boolean setInputConnectionHandler(Handler handler) {
        if (handler == mIcPostHandler) {
            return true;
        }
        if (!mFocused) {
            return false;
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
        return true;
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
        case Action.TYPE_SET_SELECTION:
            final int len = mText.length();
            final int curStart = Selection.getSelectionStart(mText);
            final int curEnd = Selection.getSelectionEnd(mText);
            // start == -1 when the start offset should remain the same
            // end == -1 when the end offset should remain the same
            final int selStart = Math.min(action.mStart < 0 ? curStart : action.mStart, len);
            final int selEnd = Math.min(action.mEnd < 0 ? curEnd : action.mEnd, len);

            if (selStart < action.mStart || selEnd < action.mEnd) {
                Log.w(LOGTAG, "IME sync error: selection out of bounds");
            }
            Selection.setSelection(mText, selStart, selEnd);
            geckoPostToIc(new Runnable() {
                @Override
                public void run() {
                    mActionQueue.syncWithGecko();
                    final int start = Selection.getSelectionStart(mText);
                    final int end = Selection.getSelectionEnd(mText);
                    if (selStart == start && selEnd == end) {
                        // There has not been another new selection in the mean time that
                        // made this notification out-of-date
                        mListener.onSelectionChange(start, end);
                    }
                }
            });
            break;

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
        if (action.mShouldUpdate) {
            geckoUpdateGecko(false);
        }
    }

    private void notifyCommitComposition() {
        // Gecko already committed its composition, and
        // we should remove the composition on our side as well.
        boolean wasComposing = false;
        final Object[] spans = mText.getSpans(0, mText.length(), Object.class);

        for (Object span : spans) {
            if ((mText.getSpanFlags(span) & Spanned.SPAN_COMPOSING) != 0) {
                mText.removeSpan(span);
                wasComposing = true;
            }
        }

        if (!wasComposing) {
            return;
        }

        // Generate a text change notification if we actually cleared the composition.
        final CharSequence text = TextUtils.stringOrSpannedString(mText);
        geckoPostToIc(new Runnable() {
            @Override
            public void run() {
                mListener.onTextChange(text, 0, text.length(), text.length());
            }
        });
    }

    private void notifyCancelComposition() {
        // Composition should have been cancelled on our side
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

    @Override
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
                // GeckoEvent.IME_ACKNOWLEDGE_FOCUS, this line also makes us
                // wait for Gecko to update us on the newly focused content
                mActionQueue.syncWithGecko();
                mListener.notifyIME(type);

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

    @Override
    public void notifyIMEContext(final int state, final String typeHint,
                          final String modeHint, final String actionHint) {
        // Because we want to be able to bind GeckoEditable to the newest LayerView instance,
        // this can be called from the Java IC thread in addition to the Gecko thread.
        if (DEBUG) {
            Log.d(LOGTAG, "notifyIMEContext(" +
                          getConstantName(GeckoEditableListener.class, "IME_STATE_", state) +
                          ", \"" + typeHint + "\", \"" + modeHint + "\", \"" + actionHint + "\")");
        }
        geckoPostToIc(new Runnable() {
            @Override
            public void run() {
                // Make sure there are no other things going on
                mActionQueue.syncWithGecko();
                // Set InputConnectionHandler in notifyIMEContext because
                // GeckoInputConnection.notifyIMEContext calls restartInput() which will invoke
                // InputConnectionHandler.onCreateInputConnection
                LayerView v = GeckoAppShell.getLayerView();
                if (v != null) {
                    mListener = GeckoInputConnection.create(v, GeckoEditable.this);
                    v.setInputConnectionHandler((InputConnectionHandler)mListener);
                    mListener.notifyIMEContext(state, typeHint, modeHint, actionHint);
                }
            }
        });
    }

    @Override
    public void onSelectionChange(final int start, final int end) {
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
        final int seqnoWhenPosted = ++mGeckoUpdateSeqno;

        /* An event (keypress, etc.) has potentially changed the selection,
           synchronize the selection here. There is not a race with the IC thread
           because the IC thread should be blocked on the event action */
        final Action action = mActionQueue.peek();
        if (action != null && action.mType == Action.TYPE_EVENT) {
            Selection.setSelection(mText, start, end);
            return;
        }

        geckoPostToIc(new Runnable() {
            @Override
            public void run() {
                mActionQueue.syncWithGecko();
                /* check to see there has not been another action that potentially changed the
                   selection. If so, we can skip this update because we know there is another
                   update right after this one that will replace the effect of this update */
                if (mGeckoUpdateSeqno == seqnoWhenPosted) {
                    /* In this case, Gecko's selection has changed and it's notifying us to change
                       Java's selection. In the normal case, whenever Java's selection changes,
                       we go back and set Gecko's selection as well. However, in this case,
                       since Gecko's selection is already up-to-date, we skip this step. */
                    boolean oldUpdateGecko = mUpdateGecko;
                    mUpdateGecko = false;
                    Selection.setSelection(mProxy, start, end);
                    mUpdateGecko = oldUpdateGecko;
                }
            }
        });
    }

    private void geckoReplaceText(int start, int oldEnd, CharSequence newText) {
        // Don't use replace() because Gingerbread has a bug where if the replaced text
        // has the same spans as the original text, the spans will end up being deleted
        mText.delete(start, oldEnd);
        mText.insert(start, newText);
    }

    private boolean isSameText(int start, int oldEnd, CharSequence newText) {
        return oldEnd - start == newText.length() &&
               TextUtils.regionMatches(mText, start, newText, 0, oldEnd - start);
    }

    @Override
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
        if (start != 0 && unboundedNewEnd != (start + text.length())) {
            Log.e(LOGTAG, "newEnd does not match text: " + unboundedNewEnd + " vs " +
                  (start + text.length()));
            throw new IllegalArgumentException("newEnd does not match text");
        }
        final int newEnd = start + text.length();
        final Action action = mActionQueue.peek();

        /* Text changes affect the selection as well, and we may not receive another selection
           update as a result of selection notification masking on the Gecko side; therefore,
           in order to prevent previous stale selection notifications from occurring, we need
           to increment the seqno here as well */
        ++mGeckoUpdateSeqno;

        if (action != null && action.mType == Action.TYPE_ACKNOWLEDGE_FOCUS) {
            // Simply replace the text for newly-focused editors.
            mText.replace(0, mText.length(), text);

        } else {
            mChangedText.clearSpans();
            mChangedText.replace(0, mChangedText.length(), text);
            // Preserve as many spans as possible
            TextUtils.copySpansFrom(mText, start, Math.min(oldEnd, newEnd),
                                    Object.class, mChangedText, 0);

            if (action != null &&
                    (action.mType == Action.TYPE_REPLACE_TEXT ||
                    action.mType == Action.TYPE_COMPOSE_TEXT) &&
                    start <= action.mStart &&
                    action.mStart + action.mSequence.length() <= newEnd) {

                // actionNewEnd is the new end of the original replacement action
                final int actionNewEnd = action.mStart + action.mSequence.length();
                int selStart = Selection.getSelectionStart(mText);
                int selEnd = Selection.getSelectionEnd(mText);

                // Replace old spans with new spans
                mChangedText.replace(action.mStart - start, actionNewEnd - start,
                                     action.mSequence);
                geckoReplaceText(start, oldEnd, mChangedText);

                // delete/insert above might have moved our selection to somewhere else
                // this happens when the Gecko text change covers a larger range than
                // the original replacement action. Fix selection here
                if (selStart >= start && selStart <= oldEnd) {
                    selStart = selStart < action.mStart ? selStart :
                               selStart < action.mEnd   ? actionNewEnd :
                                                          selStart + actionNewEnd - action.mEnd;
                    mText.setSpan(Selection.SELECTION_START, selStart, selStart,
                                  Spanned.SPAN_POINT_POINT);
                }
                if (selEnd >= start && selEnd <= oldEnd) {
                    selEnd = selEnd < action.mStart ? selEnd :
                             selEnd < action.mEnd   ? actionNewEnd :
                                                      selEnd + actionNewEnd - action.mEnd;
                    mText.setSpan(Selection.SELECTION_END, selEnd, selEnd,
                                  Spanned.SPAN_POINT_POINT);
                }

            } else {
                // Gecko side initiated the text change.
                if (isSameText(start, oldEnd, mChangedText)) {
                    // Nothing to do because the text is the same.
                    // This could happen when the composition is updated for example.
                    return;
                }
                geckoReplaceText(start, oldEnd, mChangedText);
            }
        }

        geckoPostToIc(new Runnable() {
            @Override
            public void run() {
                mListener.onTextChange(text, start, oldEnd, newEnd);
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
        mActionQueue.offer(Action.newRemoveSpan(what));
    }

    @Override
    public void setSpan(Object what, int start, int end, int flags) {
        if (what == Selection.SELECTION_START) {
            if ((flags & Spanned.SPAN_INTERMEDIATE) != 0) {
                // We will get the end offset next, just save the start for now
                mSavedSelectionStart = start;
            } else {
                mActionQueue.offer(Action.newSetSelection(start, -1));
            }
        } else if (what == Selection.SELECTION_END) {
            mActionQueue.offer(Action.newSetSelection(mSavedSelectionStart, end));
            mSavedSelectionStart = -1;
        } else {
            mActionQueue.offer(Action.newSetSpan(what, start, end, flags));
        }
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

