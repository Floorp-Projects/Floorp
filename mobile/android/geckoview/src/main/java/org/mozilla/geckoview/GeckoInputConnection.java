/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Matrix;
import android.graphics.RectF;
import android.media.AudioManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.support.annotation.NonNull;
import android.text.Editable;
import android.text.Selection;
import android.text.SpannableString;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.CursorAnchorInfo;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.ExtractedText;
import android.view.inputmethod.ExtractedTextRequest;
import android.view.inputmethod.InputConnection;

import org.mozilla.gecko.Clipboard;
import org.mozilla.gecko.InputMethods;
import org.mozilla.gecko.util.ThreadUtils;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

/* package */ final class GeckoInputConnection
    extends BaseInputConnection
    implements SessionTextInput.InputConnectionClient,
               SessionTextInput.EditableListener {

    private static final boolean DEBUG = false;
    protected static final String LOGTAG = "GeckoInputConnection";

    private static final String CUSTOM_HANDLER_TEST_METHOD = "testInputConnection";
    private static final String CUSTOM_HANDLER_TEST_CLASS =
        "org.mozilla.gecko.tests.components.GeckoViewComponent$TextInput";

    private static final int INLINE_IME_MIN_DISPLAY_SIZE = 480;

    private static Handler sBackgroundHandler;

    // Managed only by notifyIMEContext; see comments in notifyIMEContext
    private int mIMEState;
    private String mIMEActionHint = "";
    private int mLastSelectionStart;
    private int mLastSelectionEnd;

    private String mCurrentInputMethod = "";

    private final GeckoSession mSession;
    private final View mView;
    private final SessionTextInput.EditableClient mEditableClient;
    protected int mBatchEditCount;
    private ExtractedTextRequest mUpdateRequest;
    private final InputConnection mKeyInputConnection;
    private CursorAnchorInfo.Builder mCursorAnchorInfoBuilder;

    public static SessionTextInput.InputConnectionClient create(
            final GeckoSession session,
            final View targetView,
            final SessionTextInput.EditableClient editable) {
        SessionTextInput.InputConnectionClient ic = new GeckoInputConnection(session, targetView, editable);
        if (DEBUG) {
            ic = wrapForDebug(ic);
        }
        return ic;
    }

    private static SessionTextInput.InputConnectionClient wrapForDebug(final SessionTextInput.InputConnectionClient ic) {
        final InvocationHandler handler = new InvocationHandler() {
            private final StringBuilder mCallLevel = new StringBuilder();

            @Override
            public Object invoke(final Object proxy, final Method method,
                                 final Object[] args) throws Throwable {
                final StringBuilder log = new StringBuilder(mCallLevel);
                log.append("> ").append(method.getName()).append("(");
                if (args != null) {
                    for (int i = 0; i < args.length; i++) {
                        final Object arg = args[i];
                        // translate argument values to constant names
                        if ("notifyIME".equals(method.getName()) && i == 0) {
                            log.append(GeckoEditable.getConstantName(
                                    SessionTextInput.EditableListener.class,
                                    "NOTIFY_IME_", arg));
                        } else if ("notifyIMEContext".equals(method.getName()) && i == 0) {
                            log.append(GeckoEditable.getConstantName(
                                    SessionTextInput.EditableListener.class,
                                    "IME_STATE_", arg));
                        } else {
                            GeckoEditable.debugAppend(log, arg);
                        }
                        log.append(", ");
                    }
                    if (args.length > 0) {
                        log.setLength(log.length() - 2);
                    }
                }
                log.append(")");
                Log.d(LOGTAG, log.toString());

                mCallLevel.append(' ');
                Object ret = method.invoke(ic, args);
                if (ret == ic) {
                    ret = proxy;
                }
                mCallLevel.setLength(Math.max(0, mCallLevel.length() - 1));

                log.setLength(mCallLevel.length());
                log.append("< ").append(method.getName());
                if (!method.getReturnType().equals(Void.TYPE)) {
                    GeckoEditable.debugAppend(log.append(": "), ret);
                }
                Log.d(LOGTAG, log.toString());
                return ret;
            }
        };

        return (SessionTextInput.InputConnectionClient) Proxy.newProxyInstance(
                GeckoInputConnection.class.getClassLoader(),
                new Class<?>[] {
                        InputConnection.class,
                        SessionTextInput.InputConnectionClient.class,
                        SessionTextInput.EditableListener.class
                }, handler);
    }

    protected GeckoInputConnection(final GeckoSession session,
                                   final View targetView,
                                   final SessionTextInput.EditableClient editable) {
        super(targetView, true);
        mSession = session;
        mView = targetView;
        mEditableClient = editable;
        mIMEState = IME_STATE_DISABLED;
        // InputConnection that sends keys for plugins, which don't have full editors
        mKeyInputConnection = new BaseInputConnection(targetView, false);
    }

    @Override
    public synchronized boolean beginBatchEdit() {
        mBatchEditCount++;
        if (mBatchEditCount == 1) {
            mEditableClient.setBatchMode(true);
        }
        return true;
    }

    @Override
    public synchronized boolean endBatchEdit() {
        if (mBatchEditCount <= 0) {
            Log.w(LOGTAG, "endBatchEdit() called, but mBatchEditCount <= 0?!");
            return true;
        }

        mBatchEditCount--;
        if (mBatchEditCount != 0) {
            return true;
        }

        // setBatchMode will call onTextChange and/or onSelectionChange for us.
        mEditableClient.setBatchMode(false);
        return true;
    }

    @Override
    public Editable getEditable() {
        return mEditableClient.getEditable();
    }

    @Override
    public boolean performContextMenuAction(int id) {
        final View view = getView();
        final Editable editable = getEditable();
        if (view == null || editable == null) {
            return false;
        }
        int selStart = Selection.getSelectionStart(editable);
        int selEnd = Selection.getSelectionEnd(editable);

        switch (id) {
            case android.R.id.selectAll:
                setSelection(0, editable.length());
                break;
            case android.R.id.cut:
                // If selection is empty, we'll select everything
                if (selStart == selEnd) {
                    // Fill the clipboard
                    Clipboard.setText(view.getContext(), editable);
                    editable.clear();
                } else {
                    Clipboard.setText(view.getContext(),
                                      editable.subSequence(Math.min(selStart, selEnd),
                                                           Math.max(selStart, selEnd)));
                    editable.delete(selStart, selEnd);
                }
                break;
            case android.R.id.paste:
                commitText(Clipboard.getText(view.getContext()), 1);
                break;
            case android.R.id.copy:
                // Copy the current selection or the empty string if nothing is selected.
                String copiedText = selStart == selEnd ? "" :
                                    editable.toString().substring(
                                        Math.min(selStart, selEnd),
                                        Math.max(selStart, selEnd));
                Clipboard.setText(view.getContext(), copiedText);
                break;
        }
        return true;
    }

    @Override
    public ExtractedText getExtractedText(ExtractedTextRequest req, int flags) {
        if (req == null)
            return null;

        if ((flags & GET_EXTRACTED_TEXT_MONITOR) != 0)
            mUpdateRequest = req;

        Editable editable = getEditable();
        if (editable == null) {
            return null;
        }
        int selStart = Selection.getSelectionStart(editable);
        int selEnd = Selection.getSelectionEnd(editable);

        ExtractedText extract = new ExtractedText();
        extract.flags = 0;
        extract.partialStartOffset = -1;
        extract.partialEndOffset = -1;
        extract.selectionStart = selStart;
        extract.selectionEnd = selEnd;
        extract.startOffset = 0;
        if ((req.flags & GET_TEXT_WITH_STYLES) != 0) {
            extract.text = new SpannableString(editable);
        } else {
            extract.text = editable.toString();
        }
        return extract;
    }

    @Override // SessionTextInput.InputConnectionClient
    public View getView() {
        return mView;
    }

    @NonNull
    /* package */ GeckoSession.TextInputDelegate getInputDelegate() {
        return mSession.getTextInput().getDelegate();
    }

    @Override // SessionTextInput.EditableListener
    public void onTextChange() {
        final Editable editable = getEditable();
        if (mUpdateRequest == null || editable == null) {
            return;
        }

        final ExtractedTextRequest request = mUpdateRequest;
        final ExtractedText extractedText = new ExtractedText();
        extractedText.flags = 0;
        // Update the entire Editable range
        extractedText.partialStartOffset = -1;
        extractedText.partialEndOffset = -1;
        extractedText.selectionStart = Selection.getSelectionStart(editable);
        extractedText.selectionEnd = Selection.getSelectionEnd(editable);
        extractedText.startOffset = 0;
        if ((request.flags & GET_TEXT_WITH_STYLES) != 0) {
            extractedText.text = new SpannableString(editable);
        } else {
            extractedText.text = editable.toString();
        }

        getView().post(new Runnable() {
            @Override
            public void run() {
                getInputDelegate().updateExtractedText(mSession, request, extractedText);
            }
        });
    }

    @Override // SessionTextInput.EditableListener
    public void onSelectionChange() {

        final Editable editable = getEditable();
        if (editable != null) {
            mLastSelectionStart = Selection.getSelectionStart(editable);
            mLastSelectionEnd = Selection.getSelectionEnd(editable);
            notifySelectionChange(mLastSelectionStart, mLastSelectionEnd);
        }
    }

    private void notifySelectionChange(final int start, final int end) {
        final Editable editable = getEditable();
        if (editable == null) {
            return;
        }

        final int compositionStart = getComposingSpanStart(editable);
        final int compositionEnd = getComposingSpanEnd(editable);

        getView().post(new Runnable() {
            @Override
            public void run() {
                getInputDelegate().updateSelection(mSession, start, end,
                                                   compositionStart, compositionEnd);
            }
        });
    }

    @TargetApi(21)
    @Override // SessionTextInput.EditableListener
    public void updateCompositionRects(final RectF[] rects) {
        if (!(Build.VERSION.SDK_INT >= 21)) {
            return;
        }

        final View view = getView();
        if (view == null) {
            return;
        }

        final Editable content = getEditable();
        if (content == null) {
            return;
        }

        final int composingStart = getComposingSpanStart(content);
        final int composingEnd = getComposingSpanEnd(content);
        if (composingStart < 0 || composingEnd < 0) {
            if (DEBUG) {
                Log.d(LOGTAG, "No composition for updates");
            }
            return;
        }

        final CharSequence composition = content.subSequence(composingStart, composingEnd);

        view.post(new Runnable() {
            @Override
            public void run() {
                updateCompositionRectsOnUi(view, rects, composition);
            }
        });
    }

    @TargetApi(21)
    /* package */ void updateCompositionRectsOnUi(final View view,
                                                  final RectF[] rects,
                                                  final CharSequence composition) {
        if (mCursorAnchorInfoBuilder == null) {
            mCursorAnchorInfoBuilder = new CursorAnchorInfo.Builder();
        }
        mCursorAnchorInfoBuilder.reset();

        final Matrix matrix = new Matrix();
        mSession.getClientToScreenMatrix(matrix);
        mCursorAnchorInfoBuilder.setMatrix(matrix);

        for (int i = 0; i < rects.length; i++) {
            mCursorAnchorInfoBuilder.addCharacterBounds(
                    i, rects[i].left, rects[i].top, rects[i].right, rects[i].bottom,
                    CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION);
        }

        mCursorAnchorInfoBuilder.setComposingText(0, composition);

        final CursorAnchorInfo info = mCursorAnchorInfoBuilder.build();
        getView().post(new Runnable() {
            @Override
            public void run() {
                getInputDelegate().updateCursorAnchorInfo(mSession, info);
            }
        });
    }

    @Override
    public boolean requestCursorUpdates(int cursorUpdateMode) {

        if ((cursorUpdateMode & InputConnection.CURSOR_UPDATE_IMMEDIATE) != 0) {
            mEditableClient.requestCursorUpdates(
                    SessionTextInput.EditableClient.ONE_SHOT);
        }

        if ((cursorUpdateMode & InputConnection.CURSOR_UPDATE_MONITOR) != 0) {
            mEditableClient.requestCursorUpdates(
                    SessionTextInput.EditableClient.START_MONITOR);
        } else {
            mEditableClient.requestCursorUpdates(
                    SessionTextInput.EditableClient.END_MONITOR);
        }
        return true;
    }

    @Override // SessionTextInput.EditableListener
    public void onDefaultKeyEvent(final KeyEvent event) {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                GeckoInputConnection.this.performDefaultKeyAction(event);
            }
        });
    }

    private static synchronized Handler getBackgroundHandler() {
        if (sBackgroundHandler != null) {
            return sBackgroundHandler;
        }
        // Don't use GeckoBackgroundThread because Gecko thread may block waiting on
        // GeckoBackgroundThread. If we were to use GeckoBackgroundThread, due to IME,
        // GeckoBackgroundThread may end up also block waiting on Gecko thread and a
        // deadlock occurs
        Thread backgroundThread = new Thread(new Runnable() {
            @Override
            public void run() {
                Looper.prepare();
                synchronized (GeckoInputConnection.class) {
                    sBackgroundHandler = new Handler();
                    GeckoInputConnection.class.notify();
                }
                Looper.loop();
                // We should never be exiting the thread loop.
                throw new IllegalThreadStateException("unreachable code");
            }
        }, LOGTAG);
        backgroundThread.setDaemon(true);
        backgroundThread.start();
        while (sBackgroundHandler == null) {
            try {
                // wait for new thread to set sBackgroundHandler
                GeckoInputConnection.class.wait();
            } catch (InterruptedException e) {
            }
        }
        return sBackgroundHandler;
    }

    private synchronized boolean canReturnCustomHandler() {
        if (mIMEState == IME_STATE_DISABLED) {
            return false;
        }
        for (StackTraceElement frame : Thread.currentThread().getStackTrace()) {
            // We only return our custom Handler to InputMethodManager's InputConnection
            // proxy. For all other purposes, we return the regular Handler.
            // InputMethodManager retrieves the Handler for its InputConnection proxy
            // inside its method startInputInner(), so we check for that here. This is
            // valid from Android 2.2 to at least Android 4.2. If this situation ever
            // changes, we gracefully fall back to using the regular Handler.
            if ("startInputInner".equals(frame.getMethodName()) &&
                "android.view.inputmethod.InputMethodManager".equals(frame.getClassName())) {
                // Only return our own Handler to InputMethodManager and only prior to 24.
                return Build.VERSION.SDK_INT < 24;
            }
            if (CUSTOM_HANDLER_TEST_METHOD.equals(frame.getMethodName()) &&
                CUSTOM_HANDLER_TEST_CLASS.equals(frame.getClassName())) {
                // InputConnection tests should also run on the custom handler
                return true;
            }
        }
        return false;
    }

    private boolean isPhysicalKeyboardPresent() {
        final View v = getView();
        if (v == null) {
            return false;
        }
        final Configuration config = v.getContext().getResources().getConfiguration();
        return config.keyboard != Configuration.KEYBOARD_NOKEYS;
    }

    @Override // InputConnection
    public Handler getHandler() {
        final Handler handler;
        if (isPhysicalKeyboardPresent()) {
            handler = ThreadUtils.getUiHandler();
        } else {
            handler = getBackgroundHandler();
        }
        return mEditableClient.setInputConnectionHandler(handler);
    }

    @Override // SessionTextInput.InputConnectionClient
    public Handler getHandler(Handler defHandler) {
        if (!canReturnCustomHandler()) {
            return defHandler;
        }

        return getHandler();
    }

    @Override // InputConnection
    public void closeConnection() {
        // Not supported at the moment.
        super.closeConnection();
    }

    @Override // SessionTextInput.InputConnectionClient
    public synchronized InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        if (mIMEState == IME_STATE_DISABLED) {
            return null;
        }

        Context context = getView().getContext();
        DisplayMetrics metrics = context.getResources().getDisplayMetrics();
        if (Math.min(metrics.widthPixels, metrics.heightPixels) > INLINE_IME_MIN_DISPLAY_SIZE) {
            // prevent showing full-screen keyboard only when the screen is tall enough
            // to show some reasonable amount of the page (see bug 752709)
            outAttrs.imeOptions |= EditorInfo.IME_FLAG_NO_EXTRACT_UI
                                   | EditorInfo.IME_FLAG_NO_FULLSCREEN;
        }

        if (DEBUG) {
            Log.d(LOGTAG, "mapped IME states to: inputType = " +
                          Integer.toHexString(outAttrs.inputType) + ", imeOptions = " +
                          Integer.toHexString(outAttrs.imeOptions));
        }

        String prevInputMethod = mCurrentInputMethod;
        mCurrentInputMethod = InputMethods.getCurrentInputMethod(context);
        if (DEBUG) {
            Log.d(LOGTAG, "IME: CurrentInputMethod=" + mCurrentInputMethod);
        }

        outAttrs.initialSelStart = mLastSelectionStart;
        outAttrs.initialSelEnd = mLastSelectionEnd;
        return this;
    }

    private boolean replaceComposingSpanWithSelection() {
        final Editable content = getEditable();
        if (content == null) {
            return false;
        }
        int a = getComposingSpanStart(content),
            b = getComposingSpanEnd(content);
        if (a != -1 && b != -1) {
            if (DEBUG) {
                Log.d(LOGTAG, "removing composition at " + a + "-" + b);
            }
            removeComposingSpans(content);
            Selection.setSelection(content, a, b);
        }
        return true;
    }

    @Override
    public boolean commitText(CharSequence text, int newCursorPosition) {
        if (InputMethods.shouldCommitCharAsKey(mCurrentInputMethod) &&
            text.length() == 1 && newCursorPosition > 0) {
            if (DEBUG) {
                Log.d(LOGTAG, "committing \"" + text + "\" as key");
            }
            // mKeyInputConnection is a BaseInputConnection that commits text as keys;
            // but we first need to replace any composing span with a selection,
            // so that the new key events will generate characters to replace
            // text from the old composing span
            return replaceComposingSpanWithSelection() &&
                mKeyInputConnection.commitText(text, newCursorPosition);
        }
        return super.commitText(text, newCursorPosition);
    }

    @Override
    public boolean setSelection(int start, int end) {
        if (start < 0 || end < 0) {
            // Some keyboards (e.g. Samsung) can call setSelection with
            // negative offsets. In that case we ignore the call, similar to how
            // BaseInputConnection.setSelection ignores offsets that go past the length.
            return true;
        }
        return super.setSelection(start, end);
    }

    @Override
    public boolean sendKeyEvent(@NonNull KeyEvent event) {
        event = translateKey(event.getKeyCode(), event);
        mEditableClient.sendKeyEvent(getView(), event.getAction(), event);
        return false; // seems to always return false
    }

    private KeyEvent translateKey(final int keyCode, final @NonNull KeyEvent event) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_ENTER:
                if ((event.getFlags() & KeyEvent.FLAG_EDITOR_ACTION) != 0 &&
                        mIMEActionHint.equalsIgnoreCase("next")) {
                    return new KeyEvent(event.getAction(), KeyEvent.KEYCODE_TAB);
                }
                break;
        }
        return event;
    }

    // Called by OnDefaultKeyEvent handler, up from Gecko
    /* package */ void performDefaultKeyAction(KeyEvent event) {
        switch (event.getKeyCode()) {
            case KeyEvent.KEYCODE_MUTE:
            case KeyEvent.KEYCODE_HEADSETHOOK:
            case KeyEvent.KEYCODE_MEDIA_PLAY:
            case KeyEvent.KEYCODE_MEDIA_PAUSE:
            case KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE:
            case KeyEvent.KEYCODE_MEDIA_STOP:
            case KeyEvent.KEYCODE_MEDIA_NEXT:
            case KeyEvent.KEYCODE_MEDIA_PREVIOUS:
            case KeyEvent.KEYCODE_MEDIA_REWIND:
            case KeyEvent.KEYCODE_MEDIA_RECORD:
            case KeyEvent.KEYCODE_MEDIA_FAST_FORWARD:
            case KeyEvent.KEYCODE_MEDIA_CLOSE:
            case KeyEvent.KEYCODE_MEDIA_EJECT:
            case KeyEvent.KEYCODE_MEDIA_AUDIO_TRACK:
                // Forward media keypresses to the registered handler so headset controls work
                // Does the same thing as Chromium
                // https://chromium.googlesource.com/chromium/src/+/49.0.2623.67/chrome/android/java/src/org/chromium/chrome/browser/tab/TabWebContentsDelegateAndroid.java#445
                // These are all the keys dispatchMediaKeyEvent supports.
                if (Build.VERSION.SDK_INT >= 19) {
                    // dispatchMediaKeyEvent is only available on Android 4.4+
                    Context viewContext = getView().getContext();
                    AudioManager am = (AudioManager)viewContext.getSystemService(Context.AUDIO_SERVICE);
                    am.dispatchMediaKeyEvent(event);
                }
                break;
        }
    }

    @Override // SessionTextInput.EditableListener
    public void notifyIME(final int type) {
        switch (type) {
            case NOTIFY_IME_OF_FOCUS:
                // Showing/hiding vkb is done in notifyIMEContext
                if (mBatchEditCount != 0) {
                    Log.w(LOGTAG, "resetting with mBatchEditCount = " + mBatchEditCount);
                    mBatchEditCount = 0;
                }
                break;

            case NOTIFY_IME_OF_BLUR:
                break;

            default:
                if (DEBUG) {
                    throw new IllegalArgumentException("Unexpected NOTIFY_IME=" + type);
                }
                break;
        }
    }

    @Override // SessionTextInput.EditableListener
    public synchronized void notifyIMEContext(final int state, final String typeHint,
                                              final String modeHint, final String actionHint,
                                              final int flags) {
        // mIMEState and the mIME*Hint fields should only be changed by notifyIMEContext,
        // and not reset anywhere else. Usually, notifyIMEContext is called right after a
        // focus or blur, so resetting mIMEState during the focus or blur seems harmless.
        // However, this behavior is not guaranteed. Gecko may call notifyIMEContext
        // independent of focus change; that is, a focus change may not be accompanied by
        // a notifyIMEContext call. So if we reset mIMEState inside focus, there may not
        // be another notifyIMEContext call to set mIMEState to a proper value (bug 829318)
        /* When IME is 'disabled', IME processing is disabled.
           In addition, the IME UI is hidden */
        mIMEState = state;
        mIMEActionHint = (actionHint == null) ? "" : actionHint;

        // These fields are reset here and will be updated when restartInput is called below
        mUpdateRequest = null;
        mCurrentInputMethod = "";
    }
}
