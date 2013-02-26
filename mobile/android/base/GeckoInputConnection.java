/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.gfx.InputConnectionHandler;

import android.R;
import android.content.Context;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.text.Editable;
import android.text.InputType;
import android.text.Selection;
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

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.concurrent.SynchronousQueue;

class GeckoInputConnection
    extends BaseInputConnection
    implements InputConnectionHandler, GeckoEditableListener {

    private static final boolean DEBUG = false;
    protected static final String LOGTAG = "GeckoInputConnection";

    private static final int INLINE_IME_MIN_DISPLAY_SIZE = 480;

    private static Handler sBackgroundHandler;

    private class ThreadUtils {
        private Editable mUiEditable;
        private Object mUiEditableReturn;
        private Exception mUiEditableException;
        private final SynchronousQueue<Runnable> mIcRunnableSync;
        private final Runnable mIcSignalRunnable;

        public ThreadUtils() {
            mIcRunnableSync = new SynchronousQueue<Runnable>();
            mIcSignalRunnable = new Runnable() {
                @Override public void run() {
                }
            };
        }

        private void runOnIcThread(Handler icHandler, final Runnable runnable) {
            if (DEBUG) {
                GeckoApp.assertOnUiThread();
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
                GeckoApp.assertOnUiThread();
                Log.d(LOGTAG, "endWaitForUiThread()");
            }
            try {
                mIcRunnableSync.put(mIcSignalRunnable);
            } catch (InterruptedException e) {
            }
        }

        public void waitForUiThread(Handler icHandler) {
            if (DEBUG) {
                GeckoApp.assertOnThread(icHandler.getLooper().getThread());
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

        public Editable getEditableForUiThread(final Handler uiHandler,
                                               final Handler icHandler) {
            if (DEBUG) {
                GeckoApp.assertOnThread(uiHandler.getLooper().getThread());
            }
            if (icHandler.getLooper() == uiHandler.getLooper()) {
                // IC thread is UI thread; safe to use Editable directly
                return getEditable();
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
                        GeckoApp.assertOnThread(uiHandler.getLooper().getThread());
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
                                            mEditableClient.getEditable(), args);
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

    private final ThreadUtils mThreadUtils = new ThreadUtils();

    // Managed only by notifyIMEEnabled; see comments in notifyIMEEnabled
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
    private long mLastRestartInputTime;
    private final InputConnection mPluginInputConnection;

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
        // InputConnection for plugins, which don't have full editors
        mPluginInputConnection = new BaseInputConnection(targetView, false);
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
            case R.id.selectAll:
                setSelection(0, editable.length());
                break;
            case R.id.cut:
                // If selection is empty, we'll select everything
                if (selStart == selEnd) {
                    // Fill the clipboard
                    GeckoAppShell.setClipboardText(editable.toString());
                    editable.clear();
                } else {
                    GeckoAppShell.setClipboardText(
                            editable.toString().substring(
                                Math.min(selStart, selEnd),
                                Math.max(selStart, selEnd)));
                    editable.delete(selStart, selEnd);
                }
                break;
            case R.id.paste:
                commitText(GeckoAppShell.getClipboardText(), 1);
                break;
            case R.id.copy:
                // Copy the current selection or the empty string if nothing is selected.
                String copiedText = selStart == selEnd ? "" :
                                    editable.toString().substring(
                                        Math.min(selStart, selEnd),
                                        Math.max(selStart, selEnd));
                GeckoAppShell.setClipboardText(copiedText);
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
        extract.text = editable;

        return extract;
    }

    private static View getView() {
        return GeckoApp.mAppContext.getLayerView();
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

    private void tryRestartInput() {
        // Coalesce restartInput calls because InputMethodManager.restartInput()
        // is expensive and successive calls to it can lock up the keyboard
        if (SystemClock.uptimeMillis() < mLastRestartInputTime + 200) {
            return;
        }
        restartInput();
    }

    private void restartInput() {

        mLastRestartInputTime = SystemClock.uptimeMillis();

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
        imm.restartInput(v);
    }

    private void resetInputConnection() {
        if (mBatchEditCount != 0) {
            Log.w(LOGTAG, "resetting with mBatchEditCount = " + mBatchEditCount);
            mBatchEditCount = 0;
        }
        mBatchSelectionChanged = false;
        mBatchTextChanged = false;
        mUpdateRequest = null;

        mCurrentInputMethod = "";

        // Do not reset mIMEState here; see comments in notifyIMEEnabled
    }

    public void onTextChange(String text, int start, int oldEnd, int newEnd) {

        if (mUpdateRequest == null) {
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
        mUpdateExtract.text = editable;

        imm.updateExtractedText(v, mUpdateRequest.token,
                                mUpdateExtract);
    }

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
                sBackgroundHandler = null;
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
        }
        return false;
    }

    @Override
    public Handler getHandler(Handler defHandler) {
        if (!canReturnCustomHandler()) {
            return defHandler;
        }
        // getBackgroundHandler() is synchronized and requires locking,
        // but if we already have our handler, we don't have to lock
        final Handler newHandler = sBackgroundHandler != null
                                 ? sBackgroundHandler
                                 : getBackgroundHandler();
        if (mEditableClient.setInputConnectionHandler(newHandler)) {
            return newHandler;
        }
        // Setting new IC handler failed; return old IC handler
        return mEditableClient.getInputConnectionHandler();
    }

    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        if (mIMEState == IME_STATE_DISABLED) {
            return null;
        }

        outAttrs.inputType = InputType.TYPE_CLASS_TEXT;
        outAttrs.imeOptions = EditorInfo.IME_ACTION_NONE;
        outAttrs.actionLabel = null;

        if (mIMEState == IME_STATE_PASSWORD)
            outAttrs.inputType |= InputType.TYPE_TEXT_VARIATION_PASSWORD;
        else if (mIMEState == IME_STATE_PLUGIN)
            outAttrs.inputType = InputType.TYPE_NULL; // "send key events" mode
        else if (mIMETypeHint.equalsIgnoreCase("url"))
            outAttrs.inputType |= InputType.TYPE_TEXT_VARIATION_URI;
        else if (mIMETypeHint.equalsIgnoreCase("email"))
            outAttrs.inputType |= InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS;
        else if (mIMETypeHint.equalsIgnoreCase("search"))
            outAttrs.imeOptions = EditorInfo.IME_ACTION_SEARCH;
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
            outAttrs.inputType |= InputType.TYPE_TEXT_FLAG_AUTO_CORRECT;
            if (mIMEModeHint.equalsIgnoreCase("uppercase"))
                outAttrs.inputType |= InputType.TYPE_TEXT_FLAG_CAP_CHARACTERS;
            else if (mIMEModeHint.equalsIgnoreCase("titlecase"))
                outAttrs.inputType |= InputType.TYPE_TEXT_FLAG_CAP_WORDS;
            else if (mIMEModeHint.equalsIgnoreCase("autocapitalized"))
                outAttrs.inputType |= InputType.TYPE_TEXT_FLAG_CAP_SENTENCES;
            // lowercase mode is the default
        }

        if (mIMEActionHint.equalsIgnoreCase("go"))
            outAttrs.imeOptions = EditorInfo.IME_ACTION_GO;
        else if (mIMEActionHint.equalsIgnoreCase("done"))
            outAttrs.imeOptions = EditorInfo.IME_ACTION_DONE;
        else if (mIMEActionHint.equalsIgnoreCase("next"))
            outAttrs.imeOptions = EditorInfo.IME_ACTION_NEXT;
        else if (mIMEActionHint.equalsIgnoreCase("search"))
            outAttrs.imeOptions = EditorInfo.IME_ACTION_SEARCH;
        else if (mIMEActionHint.equalsIgnoreCase("send"))
            outAttrs.imeOptions = EditorInfo.IME_ACTION_SEND;
        else if (mIMEActionHint.length() > 0) {
            if (DEBUG)
                Log.w(LOGTAG, "Unexpected mIMEActionHint=\"" + mIMEActionHint + "\"");
            outAttrs.actionLabel = mIMEActionHint;
        }

        GeckoApp app = GeckoApp.mAppContext;
        DisplayMetrics metrics = app.getResources().getDisplayMetrics();
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
        mCurrentInputMethod = InputMethods.getCurrentInputMethod(app);
        if (DEBUG) {
            Log.d(LOGTAG, "IME: CurrentInputMethod=" + mCurrentInputMethod);
        }

        // If the user has changed IMEs, then notify input method observers.
        if (!mCurrentInputMethod.equals(prevInputMethod)) {
            FormAssistPopup popup = app.mFormAssistPopup;
            if (popup != null) {
                popup.onInputMethodChanged(mCurrentInputMethod);
            }
        }

        if (mIMEState == IME_STATE_PLUGIN) {
            // Since we are using a temporary string as the editable, the selection is at 0
            outAttrs.initialSelStart = 0;
            outAttrs.initialSelEnd = 0;
            return mPluginInputConnection;
        }
        Editable editable = getEditable();
        outAttrs.initialSelStart = Selection.getSelectionStart(editable);
        outAttrs.initialSelEnd = Selection.getSelectionEnd(editable);
        return this;
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
                    mThreadUtils.endWaitForUiThread();
                }
            });
            mThreadUtils.waitForUiThread(icHandler);
        }
        return false; // seems to always return false
    }

    public boolean onKeyPreIme(int keyCode, KeyEvent event) {
        return false;
    }

    public boolean onKeyDown(int keyCode, KeyEvent event) {
        return processKeyDown(keyCode, event);
    }

    private boolean processKeyDown(int keyCode, KeyEvent event) {
        if (keyCode > KeyEvent.getMaxKeyCode())
            return false;

        switch (keyCode) {
            case KeyEvent.KEYCODE_MENU:
            case KeyEvent.KEYCODE_BACK:
            case KeyEvent.KEYCODE_VOLUME_UP:
            case KeyEvent.KEYCODE_VOLUME_DOWN:
            case KeyEvent.KEYCODE_SEARCH:
                return false;
            case KeyEvent.KEYCODE_ENTER:
                if ((event.getFlags() & KeyEvent.FLAG_EDITOR_ACTION) != 0 &&
                    mIMEActionHint.equalsIgnoreCase("next"))
                    event = new KeyEvent(event.getAction(), KeyEvent.KEYCODE_TAB);
                break;
            default:
                break;
        }

        View view = getView();
        KeyListener keyListener = TextKeyListener.getInstance();

        // KeyListener returns true if it handled the event for us. KeyListener is only
        // safe to use on the UI thread; therefore we need to pass a proxy Editable to it
        if (mIMEState == IME_STATE_DISABLED ||
            mIMEState == IME_STATE_PLUGIN ||
            keyCode == KeyEvent.KEYCODE_ENTER ||
            keyCode == KeyEvent.KEYCODE_DEL ||
            keyCode == KeyEvent.KEYCODE_TAB ||
            (event.getFlags() & KeyEvent.FLAG_SOFT_KEYBOARD) != 0 ||
            view == null) {
            mEditableClient.sendEvent(GeckoEvent.createKeyEvent(event));
            return true;
        }

        Handler uiHandler = view.getRootView().getHandler();
        Handler icHandler = mEditableClient.getInputConnectionHandler();
        Editable uiEditable = mThreadUtils.getEditableForUiThread(uiHandler, icHandler);
        if (!keyListener.onKeyDown(view, uiEditable, keyCode, event)) {
            mEditableClient.sendEvent(GeckoEvent.createKeyEvent(event));
        }
        return true;
    }

    public boolean onKeyUp(int keyCode, KeyEvent event) {
        return processKeyUp(keyCode, event);
    }

    private boolean processKeyUp(int keyCode, KeyEvent event) {
        if (keyCode > KeyEvent.getMaxKeyCode())
            return false;

        switch (keyCode) {
            case KeyEvent.KEYCODE_BACK:
            case KeyEvent.KEYCODE_SEARCH:
            case KeyEvent.KEYCODE_MENU:
                return false;
            default:
                break;
        }

        View view = getView();
        KeyListener keyListener = TextKeyListener.getInstance();

        // KeyListener returns true if it handled the event for us. KeyListener is only
        // safe to use on the UI thread; therefore we need to pass a proxy Editable to it
        if (mIMEState == IME_STATE_DISABLED ||
            mIMEState == IME_STATE_PLUGIN ||
            keyCode == KeyEvent.KEYCODE_ENTER ||
            keyCode == KeyEvent.KEYCODE_DEL ||
            (event.getFlags() & KeyEvent.FLAG_SOFT_KEYBOARD) != 0 ||
            view == null) {
            mEditableClient.sendEvent(GeckoEvent.createKeyEvent(event));
            return true;
        }

        Handler uiHandler = view.getRootView().getHandler();
        Handler icHandler = mEditableClient.getInputConnectionHandler();
        Editable uiEditable = mThreadUtils.getEditableForUiThread(uiHandler, icHandler);
        if (!keyListener.onKeyUp(view, uiEditable, keyCode, event)) {
            mEditableClient.sendEvent(GeckoEvent.createKeyEvent(event));
        }
        return true;
    }

    public boolean onKeyMultiple(int keyCode, int repeatCount, KeyEvent event) {
        while ((repeatCount--) != 0) {
            if (!processKeyDown(keyCode, event) ||
                !processKeyUp(keyCode, event)) {
                return false;
            }
        }
        return true;
    }

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

    public boolean isIMEEnabled() {
        // make sure this picks up PASSWORD and PLUGIN states as well
        return mIMEState != IME_STATE_DISABLED;
    }

    public void notifyIME(final int type, final int state) {
        switch (type) {

            case NOTIFY_IME_CANCELCOMPOSITION:
                // Set composition to empty and end composition
                setComposingText("", 0);
                // Fall through

            case NOTIFY_IME_RESETINPUTSTATE:
                // Commit and end composition
                finishComposingText();
                tryRestartInput();
                break;

            case NOTIFY_IME_FOCUSCHANGE:
                // Showing/hiding vkb is done in notifyIMEEnabled
                resetInputConnection();
                break;

            default:
                if (DEBUG) {
                    throw new IllegalArgumentException("Unexpected NOTIFY_IME=" + type);
                }
                break;
        }
    }

    public void notifyIMEEnabled(final int state, final String typeHint,
                                 final String modeHint, final String actionHint) {
        // For some input type we will use a widget to display the ui, for those we must not
        // display the ime. We can display a widget for date and time types and, if the sdk version
        // is greater than 11, for datetime/month/week as well.
        if (typeHint != null &&
            (typeHint.equals("date") ||
             typeHint.equals("time") ||
             (Build.VERSION.SDK_INT > 10 && (typeHint.equals("datetime") ||
                                             typeHint.equals("month") ||
                                             typeHint.equals("week") ||
                                             typeHint.equals("datetime-local"))))) {
            mIMEState = IME_STATE_DISABLED;
            return;
        }

        // mIMEState and the mIME*Hint fields should only be changed by notifyIMEEnabled,
        // and not reset anywhere else. Usually, notifyIMEEnabled is called right after a
        // focus or blur, so resetting mIMEState during the focus or blur seems harmless.
        // However, this behavior is not guaranteed. Gecko may call notifyIMEEnabled
        // independent of focus change; that is, a focus change may not be accompanied by
        // a notifyIMEEnabled call. So if we reset mIMEState inside focus, there may not
        // be another notifyIMEEnabled call to set mIMEState to a proper value (bug 829318)
        /* When IME is 'disabled', IME processing is disabled.
           In addition, the IME UI is hidden */
        mIMEState = state;
        mIMETypeHint = (typeHint == null) ? "" : typeHint;
        mIMEModeHint = (modeHint == null) ? "" : modeHint;
        mIMEActionHint = (actionHint == null) ? "" : actionHint;

        View v = getView();
        if (v == null || !v.hasFocus()) {
            // When using Find In Page, we can still receive notifyIMEEnabled calls due to the
            // selection changing when highlighting. However in this case we don't want to reset/
            // show/hide the keyboard because the find box has the focus and is taking input from
            // the keyboard.
            return;
        }
        restartInput();
        GeckoApp.mAppContext.mMainHandler.postDelayed(new Runnable() {
            public void run() {
                if (mIMEState == IME_STATE_DISABLED) {
                    hideSoftInput();
                } else {
                    showSoftInput();
                }
            }
        }, 200); // Delay 200ms to prevent repeated IME showing/hiding
    }
}

final class DebugGeckoInputConnection
        extends GeckoInputConnection
        implements InvocationHandler {

    private InputConnection mProxy;
    private StringBuilder mCallLevel;

    private DebugGeckoInputConnection(View targetView,
                                      GeckoEditableClient editable) {
        super(targetView, editable);
        mCallLevel = new StringBuilder();
    }

    public static GeckoEditableListener create(View targetView,
                                               GeckoEditableClient editable) {
        final Class[] PROXY_INTERFACES = { InputConnection.class,
                InputConnectionHandler.class,
                GeckoEditableListener.class };
        DebugGeckoInputConnection dgic =
                new DebugGeckoInputConnection(targetView, editable);
        dgic.mProxy = (InputConnection)Proxy.newProxyInstance(
                GeckoInputConnection.class.getClassLoader(),
                PROXY_INTERFACES, dgic);
        return (GeckoEditableListener)dgic.mProxy;
    }

    public Object invoke(Object proxy, Method method, Object[] args)
            throws Throwable {

        StringBuilder log = new StringBuilder(mCallLevel);
        log.append("> ").append(method.getName()).append("(");
        for (Object arg : args) {
            // translate argument values to constant names
            if ("notifyIME".equals(method.getName()) && arg == args[0]) {
                log.append(GeckoEditable.getConstantName(
                    GeckoEditableListener.class, "NOTIFY_IME_", arg));
            } else if ("notifyIMEEnabled".equals(method.getName()) && arg == args[0]) {
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
