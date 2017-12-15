/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.mozglue.JNIObject;
import org.mozilla.gecko.util.ThreadUtils;

import android.graphics.RectF;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;
import android.view.KeyEvent;

/**
 * GeckoEditableChild implements the Gecko-facing side of IME operation. Each
 * nsWindow in the main process and each PuppetWidget in each child content
 * process has an instance of GeckoEditableChild, which communicates with the
 * GeckoEditableParent instance in the main process.
 */
final class GeckoEditableChild extends JNIObject implements IGeckoEditableChild {

    private static final boolean DEBUG = false;
    private static final String LOGTAG = "GeckoEditableChild";

    private final class RemoteChild extends IGeckoEditableChild.Stub {
        @Override // IGeckoEditableChild
        public void onKeyEvent(int action, int keyCode, int scanCode, int metaState,
                               int keyPressMetaState, long time, int domPrintableKeyValue,
                               int repeatCount, int flags, boolean isSynthesizedImeKey,
                               KeyEvent event) {
            GeckoEditableChild.this.onKeyEvent(
                    action, keyCode, scanCode, metaState, keyPressMetaState, time,
                    domPrintableKeyValue, repeatCount, flags, isSynthesizedImeKey, event);
        }

        @Override // IGeckoEditableChild
        public void onImeSynchronize() {
            GeckoEditableChild.this.onImeSynchronize();
        }

        @Override // IGeckoEditableChild
        public void onImeReplaceText(int start, int end, String text) {
            GeckoEditableChild.this.onImeReplaceText(start, end, text);
        }

        @Override // IGeckoEditableChild
        public void onImeAddCompositionRange(int start, int end, int rangeType,
                                             int rangeStyles, int rangeLineStyle,
                                             boolean rangeBoldLine, int rangeForeColor,
                                             int rangeBackColor, int rangeLineColor) {
            GeckoEditableChild.this.onImeAddCompositionRange(
                    start, end, rangeType, rangeStyles, rangeLineStyle, rangeBoldLine,
                    rangeForeColor, rangeBackColor, rangeLineColor);
        }

        @Override // IGeckoEditableChild
        public void onImeUpdateComposition(int start, int end, int flags) {
            GeckoEditableChild.this.onImeUpdateComposition(start, end, flags);
        }

        @Override // IGeckoEditableChild
        public void onImeRequestCursorUpdates(int requestMode) {
            GeckoEditableChild.this.onImeRequestCursorUpdates(requestMode);
        }
    }

    private final IGeckoEditableParent mEditableParent;
    private final IGeckoEditableChild mEditableChild;

    private int mCurrentTextLength; // Used by Gecko thread

    @WrapForJNI(calledFrom = "gecko")
    /* package */ GeckoEditableChild(final IGeckoEditableParent editableParent) {
        mEditableParent = editableParent;

        final IBinder binder = editableParent.asBinder();
        if (binder.queryLocalInterface(IGeckoEditableParent.class.getName()) != null) {
            // IGeckoEditableParent is local; i.e. we're in the main process.
            mEditableChild = this;
        } else {
            // IGeckoEditableParent is remote; i.e. we're in a content process.
            mEditableChild = new RemoteChild();
        }
    }

    @WrapForJNI(dispatchTo = "proxy") @Override // IGeckoEditableChild
    public native void onKeyEvent(int action, int keyCode, int scanCode, int metaState,
                                  int keyPressMetaState, long time, int domPrintableKeyValue,
                                  int repeatCount, int flags, boolean isSynthesizedImeKey,
                                  KeyEvent event);

    @WrapForJNI(dispatchTo = "proxy") @Override // IGeckoEditableChild
    public native void onImeSynchronize();

    @WrapForJNI(dispatchTo = "proxy") @Override // IGeckoEditableChild
    public native void onImeReplaceText(int start, int end, String text);

    @WrapForJNI(dispatchTo = "proxy") @Override // IGeckoEditableChild
    public native void onImeAddCompositionRange(int start, int end, int rangeType,
                                                int rangeStyles, int rangeLineStyle,
                                                boolean rangeBoldLine, int rangeForeColor,
                                                int rangeBackColor, int rangeLineColor);

    // Don't update to the new composition if it's different than the current composition.
    @WrapForJNI
    public static final int FLAG_KEEP_CURRENT_COMPOSITION = 1;

    @WrapForJNI(dispatchTo = "proxy") @Override // IGeckoEditableChild
    public native void onImeUpdateComposition(int start, int end, int flags);

    @WrapForJNI(dispatchTo = "proxy") @Override // IGeckoEditableChild
    public native void onImeRequestCursorUpdates(int requestMode);

    @Override // JNIObject
    protected void disposeNative() {
        // Disposal happens in native code.
        throw new UnsupportedOperationException();
    }

    @Override // IInterface
    public IBinder asBinder() {
        // Return the GeckoEditableParent's binder as our binder for comparison purposes.
        return mEditableParent.asBinder();
    }

    @WrapForJNI(calledFrom = "gecko")
    private void notifyIME(final int type) {
        if (DEBUG) {
            ThreadUtils.assertOnGeckoThread();
            Log.d(LOGTAG, "notifyIME(" + GeckoEditable.getConstantName(
                          TextInputController.EditableListener.class,
                          "NOTIFY_IME_", type) + ")");
        }
        if (type == TextInputController.EditableListener.NOTIFY_IME_TO_CANCEL_COMPOSITION) {
            // Composition should have been canceled on the parent side through text
            // update notifications. We cannot verify that here because we don't
            // keep track of spans on the child side, but it's simple to add the
            // check to the parent side if ever needed.
            return;
        }

        try {
            mEditableParent.notifyIME(mEditableChild, type);
        } catch (final RemoteException e) {
            Log.e(LOGTAG, "Remote call failed", e);
            return;
        }
    }

    @WrapForJNI(calledFrom = "gecko")
    private void notifyIMEContext(final int state, final String typeHint,
                                  final String modeHint, final String actionHint,
                                  final int flags) {
        if (DEBUG) {
            ThreadUtils.assertOnGeckoThread();
            Log.d(LOGTAG, "notifyIMEContext(" + GeckoEditable.getConstantName(
                          TextInputController.EditableListener.class,
                          "IME_STATE_", state) + ", \"" +
                          typeHint + "\", \"" + modeHint + "\", \"" + actionHint +
                          "\", 0x" + Integer.toHexString(flags) + ")");
        }

        try {
            mEditableParent.notifyIMEContext(state, typeHint, modeHint, actionHint, flags);
        } catch (final RemoteException e) {
            Log.e(LOGTAG, "Remote call failed", e);
        }
    }

    @WrapForJNI(calledFrom = "gecko", exceptionMode = "ignore")
    private void onSelectionChange(final int start, final int end) throws RemoteException {
        if (DEBUG) {
            ThreadUtils.assertOnGeckoThread();
            Log.d(LOGTAG, "onSelectionChange(" + start + ", " + end + ")");
        }

        final int currentLength = mCurrentTextLength;
        if (start < 0 || start > currentLength || end < 0 || end > currentLength) {
            Log.e(LOGTAG, "invalid selection notification range: " +
                  start + " to " + end + ", length: " + currentLength);
            throw new IllegalArgumentException("invalid selection notification range");
        }

        mEditableParent.onSelectionChange(mEditableChild.asBinder(), start, end);
    }

    @WrapForJNI(calledFrom = "gecko", exceptionMode = "ignore")
    private void onTextChange(final CharSequence text, final int start,
                              final int unboundedOldEnd, final int unboundedNewEnd)
            throws RemoteException {
        if (DEBUG) {
            ThreadUtils.assertOnGeckoThread();
            StringBuilder sb = new StringBuilder("onTextChange(");
            GeckoEditable.debugAppend(sb, text);
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
        final int currentLength = mCurrentTextLength;
        final int oldEnd = unboundedOldEnd > currentLength ? currentLength : unboundedOldEnd;
        // new end should always match text
        if (unboundedOldEnd <= currentLength && unboundedNewEnd != (start + text.length())) {
            Log.e(LOGTAG, "newEnd does not match text: " + unboundedNewEnd + " vs " +
                  (start + text.length()));
            throw new IllegalArgumentException("newEnd does not match text");
        }

        mCurrentTextLength += start + text.length() - oldEnd;
        // Need unboundedOldEnd so GeckoEditable can distinguish changed text vs cleared text.
        mEditableParent.onTextChange(mEditableChild.asBinder(), text, start, unboundedOldEnd);
    }

    @WrapForJNI(calledFrom = "gecko")
    private void onDefaultKeyEvent(final KeyEvent event) {
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

        try {
            mEditableParent.onDefaultKeyEvent(mEditableChild.asBinder(), event);
        } catch (final RemoteException e) {
            Log.e(LOGTAG, "Remote call failed", e);
        }
    }

    @WrapForJNI(calledFrom = "gecko")
    private void updateCompositionRects(final RectF[] rects) {
        if (DEBUG) {
            // GeckoEditableListener methods should all be called from the Gecko thread
            ThreadUtils.assertOnGeckoThread();
            Log.d(LOGTAG, "updateCompositionRects(rects.length = " + rects.length + ")");
        }

        try {
            mEditableParent.updateCompositionRects(mEditableChild.asBinder(), rects);
        } catch (final RemoteException e) {
            Log.e(LOGTAG, "Remote call failed", e);
        }
    }
}
