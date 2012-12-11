/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.gfx.InputConnectionHandler;
import org.mozilla.gecko.gfx.LayerView;

import android.os.Build;
import android.text.Editable;
import android.text.InputFilter;
import android.text.Spanned;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.Selection;
import android.text.TextPaint;
import android.text.TextUtils;
import android.text.style.CharacterStyle;
import android.util.Log;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.Semaphore;

// interface for the UI thread
interface GeckoEditableClient {
    void sendEvent(GeckoEvent event);
    Editable getEditable();
    void setUpdateGecko(boolean update);
}

/* interface for the Editable to listen to the Gecko thread
   and also for the UI thread to listen to the Editable */
interface GeckoEditableListener {
    // IME notification type for notifyIME()
    final int NOTIFY_IME_RESETINPUTSTATE = 0;
    final int NOTIFY_IME_REPLY_EVENT = 1;
    final int NOTIFY_IME_CANCELCOMPOSITION = 2;
    final int NOTIFY_IME_FOCUSCHANGE = 3;
    // IME focus state for notifyIME(NOTIFY_IME_FOCUSCHANGE, ..)
    final int IME_FOCUS_STATE_FOCUS = 1;
    final int IME_FOCUS_STATE_BLUR = 0;
    // IME enabled state for notifyIMEEnabled()
    final int IME_STATE_DISABLED = 0;
    final int IME_STATE_ENABLED = 1;
    final int IME_STATE_PASSWORD = 2;
    final int IME_STATE_PLUGIN = 3;

    void notifyIME(int type, int state);
    void notifyIMEEnabled(int state, String typeHint,
                          String modeHint, String actionHint);
    void onSelectionChange(int start, int end);
    void onTextChange(String text, int start, int oldEnd, int newEnd);
}

/*
   GeckoEditable implements only some functions of Editable
   The field mText contains the actual underlying
   SpannableStringBuilder/Editable that contains our text.
*/
final class GeckoEditable
        implements InvocationHandler, Editable,
                   GeckoEditableClient, GeckoEditableListener {

    private static final boolean DEBUG = false;
    private static final String LOGTAG = "GeckoEditable";

    // Filters to implement Editable's filtering functionality
    private InputFilter[] mFilters;

    private final SpannableStringBuilder mText;
    private final SpannableStringBuilder mChangedText;
    private final Editable mProxy;
    private final GeckoEditableListener mListener;
    private final ActionQueue mActionQueue;

    private int mSavedSelectionStart;
    private volatile int mGeckoUpdateSeqno;
    private int mUIUpdateSeqno;
    private int mLastUIUpdateSeqno;
    private boolean mUpdateGecko;
    private boolean mFocused;

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
           UI has updated its selection (during IME_SYNCHRONIZE reply) */
        static final int TYPE_SET_SELECTION = 2;
        // For Editable.setSpan() call; use with IME_SYNCHRONIZE
        static final int TYPE_SET_SPAN = 3;
        // For Editable.removeSpan() call; use with IME_SYNCHRONIZE
        static final int TYPE_REMOVE_SPAN = 4;

        final int mType;
        int mStart;
        int mEnd;
        CharSequence mSequence;
        Object mSpanObject;
        int mSpanFlags;
        boolean mShouldUpdate;

        Action(int type) {
            mType = type;
        }

        static Action newReplaceText(CharSequence text, int start, int end) {
            if (start < 0 || start > end) {
                throw new IllegalArgumentException("invalid replace text offsets");
            }
            final Action action = new Action(TYPE_REPLACE_TEXT);
            action.mSequence = text;
            action.mStart = start;
            action.mEnd = end;
            return action;
        }

        static Action newSetSelection(int start, int end) {
            // start == -1 when the start offset should remain the same
            // end == -1 when the end offset should remain the same
            if (start < -1 || end < -1) {
                throw new IllegalArgumentException("invalid selection offsets");
            }
            final Action action = new Action(TYPE_SET_SELECTION);
            action.mStart = start;
            action.mEnd = end;
            return action;
        }

        static Action newSetSpan(Object object, int start, int end, int flags) {
            if (start < 0 || start > end) {
                throw new IllegalArgumentException("invalid span offsets");
            }
            final Action action = new Action(TYPE_SET_SPAN);
            action.mSpanObject = object;
            action.mStart = start;
            action.mEnd = end;
            action.mSpanFlags = flags;
            return action;
        }
    }

    /* Queue of editing actions sent to Gecko thread that
       the Gecko thread has not responded to yet */
    private final class ActionQueue {
        private final ConcurrentLinkedQueue<Action> mActions;
        private final Semaphore mActionsActive;

        ActionQueue() {
            mActions = new ConcurrentLinkedQueue<Action>();
            mActionsActive = new Semaphore(1);
        }

        void offer(Action action) {
            if (DEBUG) {
                GeckoApp.assertOnUiThread();
            }
            /* Events don't need update because they generate text/selection
               notifications which will do the updating for us */
            if (action.mType != Action.TYPE_EVENT) {
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
                GeckoAppShell.sendEventToGecko(
                        GeckoEvent.createIMEEvent(GeckoEvent.IME_SYNCHRONIZE));
                break;
            case Action.TYPE_REPLACE_TEXT:
                GeckoAppShell.sendEventToGecko(GeckoEvent.createIMEReplaceEvent(
                        action.mStart, action.mEnd, action.mSequence.toString()));
                break;
            }
            ++mUIUpdateSeqno;
        }

        void poll() {
            if (DEBUG) {
                GeckoApp.assertOnGeckoThread();
            }
            if (mActions.isEmpty()) {
                throw new IllegalStateException("empty actions queue");
            }
            mActions.poll();
            // Don't bother locking if queue is not empty yet
            if (mActions.isEmpty()) {
                synchronized(this) {
                    if (mActions.isEmpty()) {
                        mActionsActive.release();
                    }
                }
            }
        }

        Action peek() {
            if (DEBUG) {
                GeckoApp.assertOnGeckoThread();
            }
            if (mActions.isEmpty()) {
                throw new IllegalStateException("empty actions queue");
            }
            return mActions.peek();
        }

        void syncWithGecko() {
            if (DEBUG) {
                GeckoApp.assertOnUiThread();
            }
            if (mFocused && !mActions.isEmpty()) {
                mActionsActive.acquireUninterruptibly();
                mActionsActive.release();
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

        LayerView v = GeckoApp.mAppContext.getLayerView();
        mListener = GeckoInputConnection.create(v, this);
    }

    private static void geckoPostToUI(Runnable runnable) {
        GeckoApp.mAppContext.mMainHandler.post(runnable);
    }

    private void geckoUpdateGecko(final boolean force) {
        /* We do not increment the seqno here, but only check it, because geckoUpdateGecko is a
           request for update. If we incremented the seqno here, geckoUpdateGecko would have
           prevented other updates from occurring */
        final int seqnoWhenPosted = mGeckoUpdateSeqno;

        geckoPostToUI(new Runnable() {
            public void run() {
                mActionQueue.syncWithGecko();
                if (seqnoWhenPosted == mGeckoUpdateSeqno) {
                    uiUpdateGecko(force);
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

    private void uiUpdateGecko(boolean force) {

        if (!force && mUIUpdateSeqno == mLastUIUpdateSeqno) {
            if (DEBUG) {
                Log.d(LOGTAG, "uiUpdateGecko() skipped");
            }
            return;
        }
        mLastUIUpdateSeqno = mUIUpdateSeqno;
        mActionQueue.syncWithGecko();

        if (DEBUG) {
            Log.d(LOGTAG, "uiUpdateGecko()");
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
            GeckoAppShell.sendEventToGecko(GeckoEvent.createIMEEvent(
                    GeckoEvent.IME_REMOVE_COMPOSITION));
            if (selStart >= 0 && selEnd >= 0) {
                GeckoAppShell.sendEventToGecko(
                        GeckoEvent.createIMESelectEvent(selStart, selEnd));
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
                // These TextPaint fields only exist on Android ICS+ and are not in the SDK
                if (Build.VERSION.SDK_INT >= 14) {
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
                Log.d(LOGTAG, " added " + rangeType + " : " + rangeStyles +
                              " : " + Integer.toHexString(rangeForeColor) +
                              " : " + Integer.toHexString(rangeBackColor));
            }
        } while (rangeStart < composingEnd);

        GeckoAppShell.sendEventToGecko(GeckoEvent.createIMECompositionEvent(
                composingStart, composingEnd));
    }

    // GeckoEditableClient interface

    @Override
    public void sendEvent(GeckoEvent event) {
        if (DEBUG) {
            // GeckoEditableClient methods should all be called from the UI thread
            GeckoApp.assertOnUiThread();
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
        if (DEBUG) {
            // GeckoEditableClient methods should all be called from the UI thread
            GeckoApp.assertOnUiThread();
        }
        return mProxy;
    }

    @Override
    public void setUpdateGecko(boolean update) {
        if (DEBUG) {
            // GeckoEditableClient methods should all be called from the UI thread
            GeckoApp.assertOnUiThread();
        }
        if (update) {
            uiUpdateGecko(false);
        }
        mUpdateGecko = update;
    }

    // GeckoEditableListener interface

    void geckoActionReply() {
        if (DEBUG) {
            // GeckoEditableListener methods should all be called from the Gecko thread
            GeckoApp.assertOnGeckoThread();
        }
        final Action action = mActionQueue.peek();

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
            geckoPostToUI(new Runnable() {
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
        }
        if (action.mShouldUpdate) {
            geckoUpdateGecko(false);
        }
        mActionQueue.poll();
    }

    @Override
    public void notifyIME(final int type, final int state) {
        if (DEBUG) {
            // GeckoEditableListener methods should all be called from the Gecko thread
            GeckoApp.assertOnGeckoThread();
        }
        if (type == NOTIFY_IME_REPLY_EVENT) {
            geckoActionReply();
            return;
        }
        geckoPostToUI(new Runnable() {
            public void run() {
                // Make sure there are no other things going on
                mActionQueue.syncWithGecko();
                if (type == NOTIFY_IME_FOCUSCHANGE) {
                    if (state == IME_FOCUS_STATE_BLUR) {
                        mFocused = false;
                    } else {
                        mFocused = true;
                        LayerView v = GeckoApp.mAppContext.getLayerView();
                        v.setInputConnectionHandler((InputConnectionHandler)mListener);
                        // Unmask events on the Gecko side
                        GeckoAppShell.sendEventToGecko(GeckoEvent.createIMEEvent(
                                GeckoEvent.IME_ACKNOWLEDGE_FOCUS));
                    }
                }
                mListener.notifyIME(type, state);
            }
        });
    }

    @Override
    public void notifyIMEEnabled(final int state, final String typeHint,
                          final String modeHint, final String actionHint) {
        if (DEBUG) {
            // GeckoEditableListener methods should all be called from the Gecko thread
            GeckoApp.assertOnGeckoThread();
        }
        geckoPostToUI(new Runnable() {
            public void run() {
                // Make sure there are no other things going on
                mActionQueue.syncWithGecko();
                mListener.notifyIMEEnabled(state, typeHint,
                                           modeHint, actionHint);
            }
        });
    }

    @Override
    public void onSelectionChange(final int start, final int end) {
        if (DEBUG) {
            // GeckoEditableListener methods should all be called from the Gecko thread
            GeckoApp.assertOnGeckoThread();
        }
        if (start < 0 || start > mText.length() || end < 0 || end > mText.length()) {
            throw new IllegalArgumentException("invalid selection notification range");
        }
        final int seqnoWhenPosted = ++mGeckoUpdateSeqno;

        /* An event (keypress, etc.) has potentially changed the selection,
           synchronize the selection here. There is not a race with the UI thread
           because the UI thread should be blocked on the event action */
        if (!mActionQueue.isEmpty() &&
            mActionQueue.peek().mType == Action.TYPE_EVENT) {
            Selection.setSelection(mText, start, end);
            return;
        }

        geckoPostToUI(new Runnable() {
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

    @Override
    public void onTextChange(final String text, final int start,
                      final int unboundedOldEnd, final int unboundedNewEnd) {
        if (DEBUG) {
            // GeckoEditableListener methods should all be called from the Gecko thread
            GeckoApp.assertOnGeckoThread();
        }
        if (start < 0 || start > unboundedOldEnd) {
            throw new IllegalArgumentException("invalid text notification range");
        }
        /* For the "end" parameters, Gecko can pass in a large
           number to denote "end of the text". Fix that here */
        final int oldEnd = unboundedOldEnd > mText.length() ? mText.length() : unboundedOldEnd;
        // new end should always match text
        if (start != 0 && unboundedNewEnd != (start + text.length())) {
            throw new IllegalArgumentException("newEnd does not match text");
        }
        final int newEnd = start + text.length();

        /* Text changes affect the selection as well, and we may not receive another selection
           update as a result of selection notification masking on the Gecko side; therefore,
           in order to prevent previous stale selection notifications from occurring, we need
           to increment the seqno here as well */
        ++mGeckoUpdateSeqno;

        mChangedText.clearSpans();
        mChangedText.replace(0, mChangedText.length(), text);
        // Preserve as many spans as possible
        TextUtils.copySpansFrom(mText, start, Math.min(oldEnd, newEnd),
                                Object.class, mChangedText, 0);

        if (!mActionQueue.isEmpty()) {
            final Action action = mActionQueue.peek();
            if (action.mType == Action.TYPE_REPLACE_TEXT &&
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
                geckoReplaceText(start, oldEnd, mChangedText);
            }
        } else {
            geckoReplaceText(start, oldEnd, mChangedText);
        }
        geckoPostToUI(new Runnable() {
            public void run() {
                mListener.onTextChange(text, start, oldEnd, newEnd);
            }
        });
    }

    // InvocationHandler interface

    private static StringBuilder debugAppend(StringBuilder sb, Object obj) {
        if (obj == null) {
            sb.append("null");
        } else if (obj instanceof GeckoEditable) {
            sb.append("GeckoEditable");
        } else if (Proxy.isProxyClass(obj.getClass())) {
            debugAppend(sb, Proxy.getInvocationHandler(obj));
        } else if (obj instanceof CharSequence) {
            sb.append("\"").append(obj.toString().replace('\n', '\u21b2')).append("\"");
        } else if (obj.getClass().isArray()) {
            sb.append(obj.getClass().getComponentType().getSimpleName()).append("[")
              .append(java.lang.reflect.Array.getLength(obj)).append("]");
        } else {
            sb.append(obj.toString());
        }
        return sb;
    }

    @Override
    public Object invoke(Object proxy, Method method, Object[] args)
                         throws Throwable {
        Object target;
        final Class<?> methodInterface = method.getDeclaringClass();
        if (DEBUG) {
            // Editable methods should all be called from the UI thread
            GeckoApp.assertOnUiThread();
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
        Object ret = method.invoke(target, args);
        if (DEBUG) {
            StringBuilder log = new StringBuilder(method.getName());
            log.append("(");
            for (Object arg : args) {
                debugAppend(log, arg).append(", ");
            }
            if (args.length > 0) {
                log.setLength(log.length() - 2);
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
        if (mText.getSpanStart(what) >= 0) { // only remove if it's there
            // Okay to remove immediately
            mText.removeSpan(what);
            mActionQueue.offer(new Action(Action.TYPE_REMOVE_SPAN));
        }
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
}

