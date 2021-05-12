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
import androidx.annotation.Nullable;
import android.util.Log;
import android.view.KeyEvent;

/**
 * GeckoEditableChild implements the Gecko-facing side of IME operation. Each
 * nsWindow in the main process and each PuppetWidget in each child content
 * process has an instance of GeckoEditableChild, which communicates with the
 * GeckoEditableParent instance in the main process.
 */
public final class GeckoEditableChild extends JNIObject implements IGeckoEditableChild {

    private static final boolean DEBUG = false;
    private static final String LOGTAG = "GeckoEditableChild";

    private static final int NOTIFY_IME_TO_CANCEL_COMPOSITION = 9;

    private final class RemoteChild extends IGeckoEditableChild.Stub {
        @Override // IGeckoEditableChild
        public void transferParent(final IGeckoEditableParent editableParent) {
            GeckoEditableChild.this.transferParent(editableParent);
        }

        @Override // IGeckoEditableChild
        public void onKeyEvent(final int action, final int keyCode, final int scanCode,
                               final int metaState, final int keyPressMetaState, final long time,
                               final int domPrintableKeyValue, final int repeatCount,
                               final int flags, final boolean isSynthesizedImeKey,
                               final KeyEvent event) {
            GeckoEditableChild.this.onKeyEvent(
                    action, keyCode, scanCode, metaState, keyPressMetaState, time,
                    domPrintableKeyValue, repeatCount, flags, isSynthesizedImeKey, event);
        }

        @Override // IGeckoEditableChild
        public void onImeSynchronize() {
            GeckoEditableChild.this.onImeSynchronize();
        }

        @Override // IGeckoEditableChild
        public void onImeReplaceText(final int start, final int end, final String text) {
            GeckoEditableChild.this.onImeReplaceText(start, end, text);
        }

        @Override // IGeckoEditableChild
        public void onImeAddCompositionRange(final int start, final int end, final int rangeType,
                                             final int rangeStyles, final int rangeLineStyle,
                                             final boolean rangeBoldLine, final int rangeForeColor,
                                             final int rangeBackColor, final int rangeLineColor) {
            GeckoEditableChild.this.onImeAddCompositionRange(
                    start, end, rangeType, rangeStyles, rangeLineStyle, rangeBoldLine,
                    rangeForeColor, rangeBackColor, rangeLineColor);
        }

        @Override // IGeckoEditableChild
        public void onImeUpdateComposition(final int start, final int end, final int flags) {
            GeckoEditableChild.this.onImeUpdateComposition(start, end, flags);
        }

        @Override // IGeckoEditableChild
        public void onImeRequestCursorUpdates(final int requestMode) {
            GeckoEditableChild.this.onImeRequestCursorUpdates(requestMode);
        }

        @Override // IGeckoEditableChild
        public void onImeRequestCommit() {
            GeckoEditableChild.this.onImeRequestCommit();
        }
    }

    private final IGeckoEditableChild mEditableChild;
    private final boolean mIsDefault;

    private IGeckoEditableParent mEditableParent;
    private int mCurrentTextLength; // Used by Gecko thread

    @WrapForJNI(calledFrom = "gecko")
    private GeckoEditableChild(@Nullable final IGeckoEditableParent editableParent,
                               final boolean isDefault) {
        mIsDefault = isDefault;

        if (editableParent != null &&
            editableParent.asBinder().queryLocalInterface(
                IGeckoEditableParent.class.getName()) != null) {
            // IGeckoEditableParent is local; i.e. we're in the main process.
            mEditableChild = this;
        } else {
            // IGeckoEditableParent is remote; i.e. we're in a content process.
            mEditableChild = new RemoteChild();
        }

        if (editableParent != null) {
            setParent(editableParent);
        }
    }

    @WrapForJNI(calledFrom = "gecko")
    private void setParent(final IGeckoEditableParent editableParent) {
        mEditableParent = editableParent;

        if (mIsDefault) {
            // Tell the parent we're the default child.
            try {
                editableParent.setDefaultChild(mEditableChild);
            } catch (final RemoteException e) {
                Log.e(LOGTAG, "Failed to set default child", e);
            }
        }
    }

    @WrapForJNI(dispatchTo = "proxy") @Override // IGeckoEditableChild
    public native void transferParent(IGeckoEditableParent editableParent);

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

    @WrapForJNI(dispatchTo = "proxy") @Override // IGeckoEditableChild
    public native void onImeRequestCommit();

    @Override // JNIObject
    protected void disposeNative() {
        // Disposal happens in native code.
        throw new UnsupportedOperationException();
    }

    @WrapForJNI(calledFrom = "gecko")
    private boolean hasEditableParent() {
        if (mEditableParent != null) {
            return true;
        }
        Log.w(LOGTAG, "No editable parent");
        return false;
    }

    @Override // IInterface
    public IBinder asBinder() {
        // Return the GeckoEditableParent's binder as fallback for comparison purposes.
        return mEditableChild != this
                ? mEditableChild.asBinder()
                : hasEditableParent()
                ? mEditableParent.asBinder() : null;
    }

    @WrapForJNI(calledFrom = "gecko")
    private void notifyIME(final int type) {
        if (DEBUG) {
            ThreadUtils.assertOnGeckoThread();
            Log.d(LOGTAG, "notifyIME(" + type + ")");
        }
        if (!hasEditableParent()) {
            return;
        }
        if (type == NOTIFY_IME_TO_CANCEL_COMPOSITION) {
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
                                  final String autocapitalize, final int flags) {
        if (DEBUG) {
            ThreadUtils.assertOnGeckoThread();
            final StringBuilder sb = new StringBuilder("notifyIMEContext(");
            sb.append(state).append(", \"").append(typeHint).append("\", \"")
                .append(modeHint).append("\", \"").append(actionHint)
                .append("\", \"").append(autocapitalize).append("\", 0x")
                .append(Integer.toHexString(flags)).append(")");
            Log.d(LOGTAG, sb.toString());
        }
        if (!hasEditableParent()) {
            return;
        }

        try {
            mEditableParent.notifyIMEContext(mEditableChild.asBinder(), state, typeHint,
                                             modeHint, actionHint, autocapitalize, flags);
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
        if (!hasEditableParent()) {
            return;
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
            Log.d(LOGTAG, "onTextChange(" + text + ", " + start + ", " +
                          unboundedOldEnd + ", " + unboundedNewEnd + ")");
        }
        if (!hasEditableParent()) {
            return;
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
        if (text.length() == 0) {
            // Remove text in range.
            mEditableParent.onTextChange(mEditableChild.asBinder(), text, start, unboundedOldEnd);
            return;
        }
        // Using large text causes TransactionTooLargeException, so split text data.
        int offset = 0;
        int newUnboundedOldEnd = unboundedOldEnd;
        while (offset < text.length()) {
            final int end = Math.min(offset + 1024 * 64 /* 64KB */, text.length());
            mEditableParent.onTextChange(mEditableChild.asBinder(), text.subSequence(offset, end), start + offset, newUnboundedOldEnd);
            offset = end;
            newUnboundedOldEnd = start + offset;
        }
    }

    @WrapForJNI(calledFrom = "gecko")
    private void onDefaultKeyEvent(final KeyEvent event) {
        if (DEBUG) {
            // GeckoEditableListener methods should all be called from the Gecko thread
            ThreadUtils.assertOnGeckoThread();
            final StringBuilder sb = new StringBuilder("onDefaultKeyEvent(");
            sb.append("action=").append(event.getAction()).append(", ")
                .append("keyCode=").append(event.getKeyCode()).append(", ")
                .append("metaState=").append(event.getMetaState()).append(", ")
                .append("time=").append(event.getEventTime()).append(", ")
                .append("repeatCount=").append(event.getRepeatCount()).append(")");
            Log.d(LOGTAG, sb.toString());
        }
        if (!hasEditableParent()) {
            return;
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
        if (!hasEditableParent()) {
            return;
        }

        try {
            mEditableParent.updateCompositionRects(mEditableChild.asBinder(), rects);
        } catch (final RemoteException e) {
            Log.e(LOGTAG, "Remote call failed", e);
        }
    }
}
