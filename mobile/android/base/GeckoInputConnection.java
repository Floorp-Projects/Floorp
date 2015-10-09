/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.concurrent.SynchronousQueue;

import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.util.Clipboard;
import org.mozilla.gecko.util.GamepadUtils;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.ThreadUtils.AssertBehavior;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.text.Editable;
import android.text.InputType;
import android.text.Selection;
import android.text.SpannableString;
import android.text.method.KeyListener;
import android.text.method.TextKeyListener;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.ExtractedText;
import android.view.inputmethod.ExtractedTextRequest;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;

class GeckoInputConnection
    extends BaseInputConnection
    implements InputConnectionListener, GeckoEditableListener {

    private static final boolean DEBUG = false;
    protected static final String LOGTAG = "GeckoInputConnection";

    private static final String CUSTOM_HANDLER_TEST_METHOD = "testInputConnection";
    private static final String CUSTOM_HANDLER_TEST_CLASS =
        "org.mozilla.gecko.tests.components.GeckoViewComponent$TextInput";

    private static final int INLINE_IME_MIN_DISPLAY_SIZE = 480;

    private static Handler sBackgroundHandler;

    private static class InputThreadUtils {
        // We only want one UI editable around to keep synchronization simple,
        // so we make InputThreadUtils a singleton
        public static final InputThreadUtils sInstance = new InputThreadUtils();

        private Editable mUiEditable;
        private Object mUiEditableReturn;
        private Exception mUiEditableException;
        private final SynchronousQueue<Runnable> mIcRunnableSync;
        private final Runnable mIcSignalRunnable;

        private InputThreadUtils() {
            mIcRunnableSync = new SynchronousQueue<Runnable>();
            mIcSignalRunnable = new Runnable() {
                @Override public void run() {
                }
            };
        }

        private void runOnIcThread(Handler icHandler, final Runnable runnable) {
            if (DEBUG) {
                ThreadUtils.assertOnUiThread();
                Log.d(LOGTAG, "runOnIcThread() on thread " +
                              icHandler.getLooper().getThread().getName());
            }
            Runnable runner = new Runnable() {
                @Override public void run() {
                    try {
                        Runnable queuedRunnable = mIcRunnableSync.take();
                        if (DEBUG && queuedRunnable != runnable) {
                            throw new IllegalThreadStateException("sync error");
                        }
                        queuedRunnable.run();
                    } catch (InterruptedException e) {
                    }
                }
            };
            try {
                // if we are not inside waitForUiThread(), runner will call the runnable
                icHandler.post(runner);
                // runnable will be called by either runner from above or waitForUiThread()
                mIcRunnableSync.put(runnable);
            } catch (InterruptedException e) {
            } finally {
                // if waitForUiThread() already called runnable, runner should not call it again
                icHandler.removeCallbacks(runner);
            }
        }

        public void endWaitForUiThread() {
            if (DEBUG) {
                ThreadUtils.assertOnUiThread();
                Log.d(LOGTAG, "endWaitForUiThread()");
            }
            try {
                mIcRunnableSync.put(mIcSignalRunnable);
            } catch (InterruptedException e) {
            }
        }

        public void waitForUiThread(Handler icHandler) {
            if (DEBUG) {
                ThreadUtils.assertOnThread(icHandler.getLooper().getThread(), AssertBehavior.THROW);
                Log.d(LOGTAG, "waitForUiThread() blocking on thread " +
                              icHandler.getLooper().getThread().getName());
            }
            try {
                Runnable runnable = null;
                do {
                    runnable = mIcRunnableSync.take();
                    runnable.run();
                } while (runnable != mIcSignalRunnable);
            } catch (InterruptedException e) {
            }
        }

        public void runOnIcThread(final Handler uiHandler,
                                  final GeckoEditableClient client,
                                  final Runnable runnable) {
            final Handler icHandler = client.getInputConnectionHandler();
            if (icHandler.getLooper() == uiHandler.getLooper()) {
                // IC thread is UI thread; safe to run directly
                runnable.run();
                return;
            }
            runOnIcThread(icHandler, runnable);
        }

        public void sendEventFromUiThread(final Handler uiHandler,
                                          final GeckoEditableClient client,
                                          final GeckoEvent event) {
            runOnIcThread(uiHandler, client, new Runnable() {
                @Override public void run() {
                    client.sendEvent(event);
                }
            });
        }

        public Editable getEditableForUiThread(final Handler uiHandler,
                                               final GeckoEditableClient client) {
            if (DEBUG) {
                ThreadUtils.assertOnThread(uiHandler.getLooper().getThread(), AssertBehavior.THROW);
            }
            final Handler icHandler = client.getInputConnectionHandler();
            if (icHandler.getLooper() == uiHandler.getLooper()) {
                // IC thread is UI thread; safe to use Editable directly
                return client.getEditable();
            }
            // IC thread is not UI thread; we need to return a proxy Editable in order
            // to safely use the Editable from the UI thread
            if (mUiEditable != null) {
                return mUiEditable;
            }
            final InvocationHandler invokeEditable = new InvocationHandler() {
                @Override public Object invoke(final Object proxy,
                                               final Method method,
                                               final Object[] args) throws Throwable {
                    if (DEBUG) {
                        ThreadUtils.assertOnThread(uiHandler.getLooper().getThread(), AssertBehavior.THROW);
                        Log.d(LOGTAG, "UiEditable." + method.getName() + "() blocking");
                    }
                    synchronized (icHandler) {
                        // Now we are on UI thread
                        mUiEditableReturn = null;
                        mUiEditableException = null;
                        // Post a Runnable that calls the real Editable and saves any
                        // result/exception. Then wait on the Runnable to finish
                        runOnIcThread(icHandler, new Runnable() {
                            @Override public void run() {
                                synchronized (icHandler) {
                                    try {
                                        mUiEditableReturn = method.invoke(
                                            client.getEditable(), args);
                                    } catch (Exception e) {
                                        mUiEditableException = e;
                                    }
                                    if (DEBUG) {
                                        Log.d(LOGTAG, "UiEditable." + method.getName() +
                                                      "() returning");
                                    }
                                    icHandler.notify();
                                }
                            }
                        });
                        // let InterruptedException propagate
                        icHandler.wait();
                        if (mUiEditableException != null) {
                            throw mUiEditableException;
                        }
                        return mUiEditableReturn;
                    }
                }
            };
            mUiEditable = (Editable) Proxy.newProxyInstance(Editable.class.getClassLoader(),
                new Class<?>[] { Editable.class }, invokeEditable);
            return mUiEditable;
        }
    }

    // Managed only by notifyIMEContext; see comments in notifyIMEContext
    private int mIMEState;
    private String mIMETypeHint = "";
    private String mIMEModeHint = "";
    private String mIMEActionHint = "";

    private String mCurrentInputMethod = "";

    private final GeckoEditableClient mEditableClient;
    protected int mBatchEditCount;
    private ExtractedTextRequest mUpdateRequest;
    private final ExtractedText mUpdateExtract = new ExtractedText();
    private boolean mBatchSelectionChanged;
    private boolean mBatchTextChanged;
    private final InputConnection mKeyInputConnection;

    public static GeckoEditableListener create(View targetView,
                                               GeckoEditableClient editable) {
        if (DEBUG)
            return DebugGeckoInputConnection.create(targetView, editable);
        else
            return new GeckoInputConnection(targetView, editable);
    }

    protected GeckoInputConnection(View targetView,
                                   GeckoEditableClient editable) {
        super(targetView, true);
        mEditableClient = editable;
        mIMEState = IME_STATE_DISABLED;
        // InputConnection that sends keys for plugins, which don't have full editors
        mKeyInputConnection = new BaseInputConnection(targetView, false);
    }

    @Override
    public synchronized boolean beginBatchEdit() {
        mBatchEditCount++;
        mEditableClient.setUpdateGecko(false);
        return true;
    }

    @Override
    public synchronized boolean endBatchEdit() {
        if (mBatchEditCount > 0) {
            mBatchEditCount--;
            if (mBatchEditCount == 0) {
                if (mBatchTextChanged) {
                    notifyTextChange();
                    mBatchTextChanged = false;
                }
                if (mBatchSelectionChanged) {
                    Editable editable = getEditable();
                    notifySelectionChange(Selection.getSelectionStart(editable),
                                           Selection.getSelectionEnd(editable));
                    mBatchSelectionChanged = false;
                }
                mEditableClient.setUpdateGecko(true);
            }
        } else {
            Log.w(LOGTAG, "endBatchEdit() called, but mBatchEditCount == 0?!");
        }
        return true;
    }

    @Override
    public Editable getEditable() {
        return mEditableClient.getEditable();
    }

    @Override
    public boolean performContextMenuAction(int id) {
        Editable editable = getEditable();
        if (editable == null) {
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
                    Clipboard.setText(editable);
                    editable.clear();
                } else {
                    Clipboard.setText(
                            editable.toString().substring(
                                Math.min(selStart, selEnd),
                                Math.max(selStart, selEnd)));
                    editable.delete(selStart, selEnd);
                }
                break;
            case android.R.id.paste:
                commitText(Clipboard.getText(), 1);
                break;
            case android.R.id.copy:
                // Copy the current selection or the empty string if nothing is selected.
                String copiedText = selStart == selEnd ? "" :
                                    editable.toString().substring(
                                        Math.min(selStart, selEnd),
                                        Math.max(selStart, selEnd));
                Clipboard.setText(copiedText);
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

    private static View getView() {
        return GeckoAppShell.getLayerView();
    }

    private static InputMethodManager getInputMethodManager() {
        View view = getView();
        if (view == null) {
            return null;
        }
        Context context = view.getContext();
        return InputMethods.getInputMethodManager(context);
    }

    private static void showSoftInput() {
        final InputMethodManager imm = getInputMethodManager();
        if (imm != null) {
            final View v = getView();
            imm.showSoftInput(v, 0);
        }
    }

    private static void hideSoftInput() {
        final InputMethodManager imm = getInputMethodManager();
        if (imm != null) {
            final View v = getView();
            imm.hideSoftInputFromWindow(v.getWindowToken(), 0);
        }
    }

    private void restartInput() {

        final InputMethodManager imm = getInputMethodManager();
        if (imm == null) {
            return;
        }
        final View v = getView();
        // InputMethodManager has internal logic to detect if we are restarting input
        // in an already focused View, which is the case here because all content text
        // fields are inside one LayerView. When this happens, InputMethodManager will
        // tell the input method to soft reset instead of hard reset. Stock latin IME
        // on Android 4.2+ has a quirk that when it soft resets, it does not clear the
        // composition. The following workaround tricks the IME into clearing the
        // composition when soft resetting.
        if (InputMethods.needsSoftResetWorkaround(mCurrentInputMethod)) {
            // Fake a selection change, because the IME clears the composition when
            // the selection changes, even if soft-resetting. Offsets here must be
            // different from the previous selection offsets, and -1 seems to be a
            // reasonable, deterministic value
            notifySelectionChange(-1, -1);
        }
        try {
            imm.restartInput(v);
        } catch(RuntimeException e) {
            Log.e(LOGTAG, "Error restarting input", e);
        }
    }

    private void resetInputConnection() {
        if (mBatchEditCount != 0) {
            Log.w(LOGTAG, "resetting with mBatchEditCount = " + mBatchEditCount);
            mBatchEditCount = 0;
        }
        mBatchSelectionChanged = false;
        mBatchTextChanged = false;

        // Do not reset mIMEState here; see comments in notifyIMEContext
    }

    @Override
    public void onTextChange(CharSequence text, int start, int oldEnd, int newEnd) {

        if (mUpdateRequest == null) {
            // Android always expects selection updates when not in extracted mode;
            // in extracted mode, the selection is reported through updateExtractedText
            final Editable editable = getEditable();
            if (editable != null) {
                onSelectionChange(Selection.getSelectionStart(editable),
                                  Selection.getSelectionEnd(editable));
            }
            return;
        }

        if (mBatchEditCount > 0) {
            // Delay notification until after the batch edit
            mBatchTextChanged = true;
            return;
        }
        notifyTextChange();
    }

    private void notifyTextChange() {

        final InputMethodManager imm = getInputMethodManager();
        final View v = getView();
        final Editable editable = getEditable();
        if (imm == null || v == null || editable == null) {
            return;
        }
        mUpdateExtract.flags = 0;
        // Update the entire Editable range
        mUpdateExtract.partialStartOffset = -1;
        mUpdateExtract.partialEndOffset = -1;
        mUpdateExtract.selectionStart =
                Selection.getSelectionStart(editable);
        mUpdateExtract.selectionEnd =
                Selection.getSelectionEnd(editable);
        mUpdateExtract.startOffset = 0;
        if ((mUpdateRequest.flags & GET_TEXT_WITH_STYLES) != 0) {
            mUpdateExtract.text = new SpannableString(editable);
        } else {
            mUpdateExtract.text = editable.toString();
        }
        imm.updateExtractedText(v, mUpdateRequest.token,
                                mUpdateExtract);
    }

    @Override
    public void onSelectionChange(int start, int end) {

        if (mBatchEditCount > 0) {
            // Delay notification until after the batch edit
            mBatchSelectionChanged = true;
            return;
        }
        notifySelectionChange(start, end);
    }

    private void notifySelectionChange(int start, int end) {

        final InputMethodManager imm = getInputMethodManager();
        final View v = getView();
        final Editable editable = getEditable();
        if (imm == null || v == null || editable == null) {
            return;
        }
        imm.updateSelection(v, start, end, getComposingSpanStart(editable),
                            getComposingSpanEnd(editable));
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

    private boolean canReturnCustomHandler() {
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
                // only return our own Handler to InputMethodManager
                return true;
            }
            if (CUSTOM_HANDLER_TEST_METHOD.equals(frame.getMethodName()) &&
                CUSTOM_HANDLER_TEST_CLASS.equals(frame.getClassName())) {
                // InputConnection tests should also run on the custom handler
                return true;
            }
        }
        return false;
    }

    @Override
    public Handler getHandler(Handler defHandler) {
        if (!canReturnCustomHandler()) {
            return defHandler;
        }
        final Handler newHandler = getBackgroundHandler();
        if (mEditableClient.setInputConnectionHandler(newHandler)) {
            return newHandler;
        }
        // Setting new IC handler failed; return old IC handler
        return mEditableClient.getInputConnectionHandler();
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        if (mIMEState == IME_STATE_DISABLED) {
            return null;
        }

        outAttrs.inputType = InputType.TYPE_CLASS_TEXT;
        outAttrs.imeOptions = EditorInfo.IME_ACTION_NONE;
        outAttrs.actionLabel = null;

        if (mIMEState == IME_STATE_PASSWORD ||
            "password".equalsIgnoreCase(mIMETypeHint))
            outAttrs.inputType |= InputType.TYPE_TEXT_VARIATION_PASSWORD;
        else if (mIMEState == IME_STATE_PLUGIN)
            outAttrs.inputType = InputType.TYPE_NULL; // "send key events" mode
        else if (mIMETypeHint.equalsIgnoreCase("url"))
            outAttrs.inputType |= InputType.TYPE_TEXT_VARIATION_URI;
        else if (mIMETypeHint.equalsIgnoreCase("email"))
            outAttrs.inputType |= InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS;
        else if (mIMETypeHint.equalsIgnoreCase("tel"))
            outAttrs.inputType = InputType.TYPE_CLASS_PHONE;
        else if (mIMETypeHint.equalsIgnoreCase("number") ||
                 mIMETypeHint.equalsIgnoreCase("range"))
            outAttrs.inputType = InputType.TYPE_CLASS_NUMBER
                                 | InputType.TYPE_NUMBER_FLAG_SIGNED
                                 | InputType.TYPE_NUMBER_FLAG_DECIMAL;
        else if (mIMETypeHint.equalsIgnoreCase("week") ||
                 mIMETypeHint.equalsIgnoreCase("month"))
            outAttrs.inputType = InputType.TYPE_CLASS_DATETIME
                                  | InputType.TYPE_DATETIME_VARIATION_DATE;
        else if (mIMEModeHint.equalsIgnoreCase("numeric"))
            outAttrs.inputType = InputType.TYPE_CLASS_NUMBER |
                                 InputType.TYPE_NUMBER_FLAG_SIGNED |
                                 InputType.TYPE_NUMBER_FLAG_DECIMAL;
        else if (mIMEModeHint.equalsIgnoreCase("digit"))
            outAttrs.inputType = InputType.TYPE_CLASS_NUMBER;
        else {
            // TYPE_TEXT_FLAG_IME_MULTI_LINE flag makes the fullscreen IME line wrap
            outAttrs.inputType |= InputType.TYPE_TEXT_FLAG_AUTO_CORRECT |
                                  InputType.TYPE_TEXT_FLAG_IME_MULTI_LINE;
            if (mIMETypeHint.equalsIgnoreCase("textarea") ||
                    mIMETypeHint.length() == 0) {
                // empty mIMETypeHint indicates contentEditable/designMode documents
                outAttrs.inputType |= InputType.TYPE_TEXT_FLAG_MULTI_LINE;
            }
            if (mIMEModeHint.equalsIgnoreCase("uppercase"))
                outAttrs.inputType |= InputType.TYPE_TEXT_FLAG_CAP_CHARACTERS;
            else if (mIMEModeHint.equalsIgnoreCase("titlecase"))
                outAttrs.inputType |= InputType.TYPE_TEXT_FLAG_CAP_WORDS;
            else if (mIMETypeHint.equalsIgnoreCase("text") &&
                    !mIMEModeHint.equalsIgnoreCase("autocapitalized"))
                outAttrs.inputType |= InputType.TYPE_TEXT_VARIATION_NORMAL;
            else if (!mIMEModeHint.equalsIgnoreCase("lowercase"))
                outAttrs.inputType |= InputType.TYPE_TEXT_FLAG_CAP_SENTENCES;
            // auto-capitalized mode is the default for types other than text
        }

        if (mIMEActionHint.equalsIgnoreCase("go"))
            outAttrs.imeOptions = EditorInfo.IME_ACTION_GO;
        else if (mIMEActionHint.equalsIgnoreCase("done"))
            outAttrs.imeOptions = EditorInfo.IME_ACTION_DONE;
        else if (mIMEActionHint.equalsIgnoreCase("next"))
            outAttrs.imeOptions = EditorInfo.IME_ACTION_NEXT;
        else if (mIMEActionHint.equalsIgnoreCase("search") ||
                 mIMETypeHint.equalsIgnoreCase("search"))
            outAttrs.imeOptions = EditorInfo.IME_ACTION_SEARCH;
        else if (mIMEActionHint.equalsIgnoreCase("send"))
            outAttrs.imeOptions = EditorInfo.IME_ACTION_SEND;
        else if (mIMEActionHint.length() > 0) {
            if (DEBUG)
                Log.w(LOGTAG, "Unexpected mIMEActionHint=\"" + mIMEActionHint + "\"");
            outAttrs.actionLabel = mIMEActionHint;
        }

        Context context = GeckoAppShell.getContext();
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

        // If the user has changed IMEs, then notify input method observers.
        if (!mCurrentInputMethod.equals(prevInputMethod) && GeckoAppShell.getGeckoInterface() != null) {
            FormAssistPopup popup = GeckoAppShell.getGeckoInterface().getFormAssistPopup();
            if (popup != null) {
                popup.onInputMethodChanged(mCurrentInputMethod);
            }
        }

        if (mIMEState == IME_STATE_PLUGIN) {
            // Since we are using a temporary string as the editable, the selection is at 0
            outAttrs.initialSelStart = 0;
            outAttrs.initialSelEnd = 0;
            return mKeyInputConnection;
        }
        Editable editable = getEditable();
        outAttrs.initialSelStart = Selection.getSelectionStart(editable);
        outAttrs.initialSelEnd = Selection.getSelectionEnd(editable);
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
    public boolean sendKeyEvent(KeyEvent event) {
        // BaseInputConnection.sendKeyEvent() dispatches the key event to the main thread.
        // In order to ensure events are processed in the proper order, we must block the
        // IC thread until the main thread finishes processing the key event
        super.sendKeyEvent(event);
        final View v = getView();
        if (v == null) {
            return false;
        }
        final Handler icHandler = mEditableClient.getInputConnectionHandler();
        final Handler mainHandler = v.getRootView().getHandler();
        if (icHandler.getLooper() != mainHandler.getLooper()) {
            // We are on separate IC thread but the event is queued on the main thread;
            // wait on IC thread until the main thread processes our posted Runnable. At
            // that point the key event has already been processed.
            mainHandler.post(new Runnable() {
                @Override public void run() {
                    InputThreadUtils.sInstance.endWaitForUiThread();
                }
            });
            InputThreadUtils.sInstance.waitForUiThread(icHandler);
        }
        return false; // seems to always return false
    }

    @Override
    public boolean onKeyPreIme(int keyCode, KeyEvent event) {
        return false;
    }

    private boolean shouldProcessKey(int keyCode, KeyEvent event) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_MENU:
            case KeyEvent.KEYCODE_BACK:
            case KeyEvent.KEYCODE_VOLUME_UP:
            case KeyEvent.KEYCODE_VOLUME_DOWN:
            case KeyEvent.KEYCODE_SEARCH:
                return false;
        }
        return true;
    }

    private boolean shouldSkipKeyListener(int keyCode, KeyEvent event) {
        if (mIMEState == IME_STATE_DISABLED ||
            mIMEState == IME_STATE_PLUGIN) {
            return true;
        }
        // Preserve enter and tab keys for the browser
        if (keyCode == KeyEvent.KEYCODE_ENTER ||
            keyCode == KeyEvent.KEYCODE_TAB) {
            return true;
        }
        // BaseKeyListener returns false even if it handled these keys for us,
        // so we skip the key listener entirely and handle these ourselves
        if (keyCode == KeyEvent.KEYCODE_DEL ||
            keyCode == KeyEvent.KEYCODE_FORWARD_DEL) {
            return true;
        }
        return false;
    }

    private KeyEvent translateKey(int keyCode, KeyEvent event) {
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

    private boolean processKey(int keyCode, KeyEvent event, boolean down) {
        if (GamepadUtils.isSonyXperiaGamepadKeyEvent(event)) {
            event = GamepadUtils.translateSonyXperiaGamepadKeys(keyCode, event);
            keyCode = event.getKeyCode();
        }

        if (keyCode > KeyEvent.getMaxKeyCode() ||
            !shouldProcessKey(keyCode, event)) {
            return false;
        }
        final int action = down ? KeyEvent.ACTION_DOWN : KeyEvent.ACTION_UP;
        event = translateKey(keyCode, event);
        keyCode = event.getKeyCode();

        View view = getView();
        if (view == null) {
            InputThreadUtils.sInstance.sendEventFromUiThread(ThreadUtils.getUiHandler(),
                mEditableClient, GeckoEvent.createKeyEvent(event, action, 0));
            return true;
        }

        // KeyListener returns true if it handled the event for us. KeyListener is only
        // safe to use on the UI thread; therefore we need to pass a proxy Editable to it
        KeyListener keyListener = TextKeyListener.getInstance();
        Handler uiHandler = view.getRootView().getHandler();
        Editable uiEditable = InputThreadUtils.sInstance.
            getEditableForUiThread(uiHandler, mEditableClient);
        boolean skip = shouldSkipKeyListener(keyCode, event);
        if (down) {
            mEditableClient.setSuppressKeyUp(true);
        }
        if (skip ||
            (down && !keyListener.onKeyDown(view, uiEditable, keyCode, event)) ||
            (!down && !keyListener.onKeyUp(view, uiEditable, keyCode, event))) {
            InputThreadUtils.sInstance.sendEventFromUiThread(uiHandler, mEditableClient,
                GeckoEvent.createKeyEvent(event, action, TextKeyListener.getMetaState(uiEditable)));
            if (skip && down) {
                // Usually, the down key listener call above adjusts meta states for us.
                // However, if we skip that call above, we have to manually adjust meta
                // states so the meta states remain consistent
                TextKeyListener.adjustMetaAfterKeypress(uiEditable);
            }
        }
        if (down) {
            mEditableClient.setSuppressKeyUp(false);
        }
        return true;
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        return processKey(keyCode, event, true);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        return processKey(keyCode, event, false);
    }

    /**
     * Get a key that represents a given character.
     */
    private KeyEvent getCharKeyEvent(final char c) {
        final long time = SystemClock.uptimeMillis();
        return new KeyEvent(time, time, KeyEvent.ACTION_MULTIPLE,
                            KeyEvent.KEYCODE_UNKNOWN, /* repeat */ 0) {
            @Override
            public int getUnicodeChar() {
                return c;
            }

            @Override
            public int getUnicodeChar(int metaState) {
                return c;
            }
        };
    }

    @Override
    public boolean onKeyMultiple(int keyCode, int repeatCount, final KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_UNKNOWN) {
            // KEYCODE_UNKNOWN means the characters are in KeyEvent.getCharacters()
            final String str = event.getCharacters();
            for (int i = 0; i < str.length(); i++) {
                final KeyEvent charEvent = getCharKeyEvent(str.charAt(i));
                if (!processKey(KeyEvent.KEYCODE_UNKNOWN, charEvent, /* down */ true) ||
                    !processKey(KeyEvent.KEYCODE_UNKNOWN, charEvent, /* down */ false)) {
                    return false;
                }
            }
            return true;
        }

        while ((repeatCount--) != 0) {
            if (!processKey(keyCode, event, /* down */ true) ||
                !processKey(keyCode, event, /* down */ false)) {
                return false;
            }
        }
        return true;
    }

    @Override
    public boolean onKeyLongPress(int keyCode, KeyEvent event) {
        View v = getView();
        switch (keyCode) {
            case KeyEvent.KEYCODE_MENU:
                InputMethodManager imm = getInputMethodManager();
                imm.toggleSoftInputFromWindow(v.getWindowToken(),
                                              InputMethodManager.SHOW_FORCED, 0);
                return true;
            default:
                break;
        }
        return false;
    }

    @Override
    public boolean isIMEEnabled() {
        // make sure this picks up PASSWORD and PLUGIN states as well
        return mIMEState != IME_STATE_DISABLED;
    }

    @Override
    public void notifyIME(int type) {
        switch (type) {

            case NOTIFY_IME_OF_FOCUS:
            case NOTIFY_IME_OF_BLUR:
                // Showing/hiding vkb is done in notifyIMEContext
                resetInputConnection();
                break;

            case NOTIFY_IME_OPEN_VKB:
                showSoftInput();
                break;

            default:
                if (DEBUG) {
                    throw new IllegalArgumentException("Unexpected NOTIFY_IME=" + type);
                }
                break;
        }
    }

    @Override
    public void notifyIMEContext(int state, String typeHint, String modeHint, String actionHint) {
        // For some input type we will use a widget to display the ui, for those we must not
        // display the ime. We can display a widget for date and time types and, if the sdk version
        // is 11 or greater, for datetime/month/week as well.
        if (typeHint != null &&
            (typeHint.equalsIgnoreCase("date") ||
             typeHint.equalsIgnoreCase("time") ||
             (Versions.feature11Plus && (typeHint.equalsIgnoreCase("datetime") ||
                                         typeHint.equalsIgnoreCase("month") ||
                                         typeHint.equalsIgnoreCase("week") ||
                                         typeHint.equalsIgnoreCase("datetime-local"))))) {
            state = IME_STATE_DISABLED;
        }

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
        mIMETypeHint = (typeHint == null) ? "" : typeHint;
        mIMEModeHint = (modeHint == null) ? "" : modeHint;
        mIMEActionHint = (actionHint == null) ? "" : actionHint;

        // These fields are reset here and will be updated when restartInput is called below
        mUpdateRequest = null;
        mCurrentInputMethod = "";

        View v = getView();
        if (v == null || !v.hasFocus()) {
            // When using Find In Page, we can still receive notifyIMEContext calls due to the
            // selection changing when highlighting. However in this case we don't want to reset/
            // show/hide the keyboard because the find box has the focus and is taking input from
            // the keyboard.
            return;
        }
        restartInput();
        if (mIMEState == IME_STATE_DISABLED) {
            hideSoftInput();
        } else {
            showSoftInput();
        }
    }
}

final class DebugGeckoInputConnection
        extends GeckoInputConnection
        implements InvocationHandler {

    private InputConnection mProxy;
    private final StringBuilder mCallLevel;

    private DebugGeckoInputConnection(View targetView,
                                      GeckoEditableClient editable) {
        super(targetView, editable);
        mCallLevel = new StringBuilder();
    }

    public static GeckoEditableListener create(View targetView,
                                               GeckoEditableClient editable) {
        final Class<?>[] PROXY_INTERFACES = { InputConnection.class,
                InputConnectionListener.class,
                GeckoEditableListener.class };
        DebugGeckoInputConnection dgic =
                new DebugGeckoInputConnection(targetView, editable);
        dgic.mProxy = (InputConnection)Proxy.newProxyInstance(
                GeckoInputConnection.class.getClassLoader(),
                PROXY_INTERFACES, dgic);
        return (GeckoEditableListener)dgic.mProxy;
    }

    @Override
    public Object invoke(Object proxy, Method method, Object[] args)
            throws Throwable {

        StringBuilder log = new StringBuilder(mCallLevel);
        log.append("> ").append(method.getName()).append("(");
        if (args != null) {
            for (Object arg : args) {
                // translate argument values to constant names
                if ("notifyIME".equals(method.getName()) && arg == args[0]) {
                    log.append(GeckoEditable.getConstantName(
                        GeckoEditableListener.class, "NOTIFY_IME_", arg));
                } else if ("notifyIMEContext".equals(method.getName()) && arg == args[0]) {
                    log.append(GeckoEditable.getConstantName(
                        GeckoEditableListener.class, "IME_STATE_", arg));
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
        Object ret = method.invoke(this, args);
        if (ret == this) {
            ret = mProxy;
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
}
