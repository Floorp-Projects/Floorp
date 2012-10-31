/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.gfx.InputConnectionHandler;

import android.R;
import android.content.Context;
import android.os.Build;
import android.os.SystemClock;
import android.text.Editable;
import android.text.InputType;
import android.text.Selection;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.text.method.KeyListener;
import android.text.method.TextKeyListener;
import android.text.style.BackgroundColorSpan;
import android.text.style.CharacterStyle;
import android.text.style.ForegroundColorSpan;
import android.text.style.UnderlineSpan;
import android.util.DisplayMetrics;
import android.util.Log;
import android.util.LogPrinter;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.CompletionInfo;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.ExtractedText;
import android.view.inputmethod.ExtractedTextRequest;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;

import java.util.Timer;
import java.util.TimerTask;

class GeckoInputConnection
    extends BaseInputConnection
    implements InputConnectionHandler, GeckoEditableListener {

    private static final boolean DEBUG = false;
    protected static final String LOGTAG = "GeckoInputConnection";

    // IME stuff
    public static final int IME_STATE_DISABLED = 0;
    public static final int IME_STATE_ENABLED = 1;
    public static final int IME_STATE_PASSWORD = 2;
    public static final int IME_STATE_PLUGIN = 3;

    private static final int NOTIFY_IME_RESETINPUTSTATE = 0;
    private static final int NOTIFY_IME_SETOPENSTATE = 1;
    private static final int NOTIFY_IME_CANCELCOMPOSITION = 2;
    private static final int NOTIFY_IME_FOCUSCHANGE = 3;

    private static final int NO_COMPOSITION_STRING = -1;

    private static final int INLINE_IME_MIN_DISPLAY_SIZE = 480;

    private static final char UNICODE_BULLET                    = '\u2022';
    private static final char UNICODE_CENT_SIGN                 = '\u00a2';
    private static final char UNICODE_COPYRIGHT_SIGN            = '\u00a9';
    private static final char UNICODE_CRARR                     = '\u21b2'; // &crarr;
    private static final char UNICODE_DIVISION_SIGN             = '\u00f7';
    private static final char UNICODE_DOUBLE_LOW_QUOTATION_MARK = '\u201e';
    private static final char UNICODE_ELLIPSIS                  = '\u2026';
    private static final char UNICODE_EURO_SIGN                 = '\u20ac';
    private static final char UNICODE_INVERTED_EXCLAMATION_MARK = '\u00a1';
    private static final char UNICODE_MULTIPLICATION_SIGN       = '\u00d7';
    private static final char UNICODE_PI                        = '\u03a0';
    private static final char UNICODE_PILCROW_SIGN              = '\u00b6';
    private static final char UNICODE_POUND_SIGN                = '\u00a3';
    private static final char UNICODE_REGISTERED_SIGN           = '\u00ae';
    private static final char UNICODE_SQUARE_ROOT               = '\u221a';
    private static final char UNICODE_TRADEMARK_SIGN            = '\u2122';
    private static final char UNICODE_WHITE_BULLET              = '\u25e6';
    private static final char UNICODE_YEN_SIGN                  = '\u00a5';

    private static final Timer mIMETimer = new Timer("GeckoInputConnection Timer");
    private static int mIMEState;
    private static String mIMETypeHint = "";
    private static String mIMEModeHint = "";
    private static String mIMEActionHint = "";

    private String mCurrentInputMethod;

    // Is a composition active?
    private int mCompositionStart = NO_COMPOSITION_STRING;
    private boolean mCommittingText;
    private KeyCharacterMap mKeyCharacterMap;
    private final Editable mEditable;
    private final GeckoEditableClient mEditableClient;
    protected int mBatchEditCount;
    private ExtractedTextRequest mUpdateRequest;
    private final ExtractedText mUpdateExtract = new ExtractedText();

    public static InputConnectionHandler create(View targetView,
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
        // install the editable => input connection listener
        editable.setListener(this);
        mIMEState = IME_STATE_DISABLED;
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

    private static void postToUiThread(Runnable runnable) {
        // postToUiThread() is called by the Gecko and TimerTask threads.
        // The UI thread does not need to post Runnables to itself.
        GeckoApp.mAppContext.mMainHandler.post(runnable);
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

    public void onTextChange(String text, int start, int oldEnd, int newEnd) {

        if (mBatchEditCount > 0 || mUpdateRequest == null) {
            return;
        }

        final InputMethodManager imm = getInputMethodManager();
        if (imm == null) {
            return;
        }
        final View v = getView();
        final Editable editable = getEditable();

        mUpdateExtract.flags = 0;
        // Update from (0, oldEnd) to (0, newEnd) because some IMEs
        // assume that updates start at zero, according to jchen.
        mUpdateExtract.partialStartOffset = 0;
        mUpdateExtract.partialEndOffset = editable.length();
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
            return;
        }
        final InputMethodManager imm = getInputMethodManager();
        if (imm == null) {
            return;
        }
        final View v = getView();
        final Editable editable = getEditable();
        imm.updateSelection(v, start, end, getComposingSpanStart(editable),
                            getComposingSpanEnd(editable));
    }

    protected void resetCompositionState() {
        if (mBatchEditCount > 0) {
            Log.d(LOGTAG, "resetCompositionState: resetting mBatchEditCount "
                          + mBatchEditCount + " -> 0");
            mBatchEditCount = 0;
        }

        removeComposingSpans(getEditable());
        mUpdateRequest = null;
    }

    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        outAttrs.inputType = InputType.TYPE_CLASS_TEXT;
        outAttrs.imeOptions = EditorInfo.IME_ACTION_NONE;
        outAttrs.actionLabel = null;

        if (mIMEState == IME_STATE_PASSWORD)
            outAttrs.inputType |= InputType.TYPE_TEXT_VARIATION_PASSWORD;
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

        String prevInputMethod = mCurrentInputMethod;
        mCurrentInputMethod = InputMethods.getCurrentInputMethod(app);
        if (DEBUG) {
            Log.d(LOGTAG, "IME: CurrentInputMethod=" + mCurrentInputMethod);
        }

        // If the user has changed IMEs, then notify input method observers.
        if (mCurrentInputMethod != prevInputMethod) {
            FormAssistPopup popup = app.mFormAssistPopup;
            if (popup != null) {
                popup.onInputMethodChanged(mCurrentInputMethod);
            }
        }

        resetCompositionState();
        return this;
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

        // KeyListener returns true if it handled the event for us.
        if (mIMEState == IME_STATE_DISABLED ||
                keyCode == KeyEvent.KEYCODE_ENTER ||
                keyCode == KeyEvent.KEYCODE_DEL ||
                keyCode == KeyEvent.KEYCODE_TAB ||
                (event.getFlags() & KeyEvent.FLAG_SOFT_KEYBOARD) != 0 ||
                !keyListener.onKeyDown(view, getEditable(), keyCode, event)) {
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

        if (mIMEState == IME_STATE_DISABLED ||
            keyCode == KeyEvent.KEYCODE_ENTER ||
            keyCode == KeyEvent.KEYCODE_DEL ||
            (event.getFlags() & KeyEvent.FLAG_SOFT_KEYBOARD) != 0 ||
            !keyListener.onKeyUp(view, getEditable(), keyCode, event)) {
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

        final View v = getView();
        if (v == null)
            return;

        switch (type) {
            case NOTIFY_IME_RESETINPUTSTATE:
                if (DEBUG) Log.d(LOGTAG, ". . . notifyIME: reset");

                resetCompositionState();

                // Don't use IMEStateUpdater for reset.
                // Because IME may not work showSoftInput()
                // after calling restartInput() immediately.
                // So we have to call showSoftInput() delay.
                InputMethodManager imm = getInputMethodManager();
                if (imm == null) {
                    // no way to reset IME status directly
                    IMEStateUpdater.resetIME();
                } else {
                    imm.restartInput(v);
                }

                // keep current enabled state
                IMEStateUpdater.enableIME();
                break;

            case NOTIFY_IME_CANCELCOMPOSITION:
                if (DEBUG) Log.d(LOGTAG, ". . . notifyIME: cancel");
                removeComposingSpans(getEditable());
                break;

            case NOTIFY_IME_FOCUSCHANGE:
                if (DEBUG) Log.d(LOGTAG, ". . . notifyIME: focus");
                IMEStateUpdater.resetIME();
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
        // For some input type we will use a  widget to display the ui, for those we must not
        // display the ime. We can display a widget for date and time types and, if the sdk version
        // is greater than 11, for datetime/month/week as well.
        if (typeHint.equals("date") || typeHint.equals("time") ||
            (Build.VERSION.SDK_INT > 10 &&
            (typeHint.equals("datetime") || typeHint.equals("month") ||
            typeHint.equals("week") || typeHint.equals("datetime-local")))) {
            return;
        }

        final View v = getView();
        if (v == null)
            return;

        /* When IME is 'disabled', IME processing is disabled.
           In addition, the IME UI is hidden */
        mIMEState = state;
        mIMETypeHint = (typeHint == null) ? "" : typeHint;
        mIMEModeHint = (modeHint == null) ? "" : modeHint;
        mIMEActionHint = (actionHint == null) ? "" : actionHint;
        IMEStateUpdater.enableIME();
    }

    /* Delay updating IME states (see bug 573800) */
    private static final class IMEStateUpdater extends TimerTask {
        private static IMEStateUpdater instance;
        private boolean mEnable;
        private boolean mReset;

        private static IMEStateUpdater getInstance() {
            if (instance == null) {
                instance = new IMEStateUpdater();
                mIMETimer.schedule(instance, 200);
            }
            return instance;
        }

        public static synchronized void enableIME() {
            getInstance().mEnable = true;
        }

        public static synchronized void resetIME() {
            getInstance().mReset = true;
        }

        public void run() {
            if (DEBUG) Log.d(LOGTAG, "IME: IMEStateUpdater.run()");
            synchronized (IMEStateUpdater.class) {
                instance = null;
            }

            // TimerTask.run() is running on a random background thread, so post to UI thread.
            postToUiThread(new Runnable() {
                public void run() {
                    final View v = getView();
                    if (v == null)
                        return;

                    final InputMethodManager imm = getInputMethodManager();
                    if (imm == null)
                        return;

                    if (mReset)
                        imm.restartInput(v);

                    if (!mEnable)
                        return;

                    if (mIMEState != IME_STATE_DISABLED) {
                        imm.showSoftInput(v, 0);
                    } else if (imm.isActive(v)) {
                        imm.hideSoftInputFromWindow(v.getWindowToken(), 0);
                    }
                }
            });
        }
    }

private static final class DebugGeckoInputConnection extends GeckoInputConnection {
    public DebugGeckoInputConnection(View targetView) {
        super(targetView);
        GeckoApp.assertOnUiThread();
    }

    @Override
    public boolean beginBatchEdit() {
        Log.d(LOGTAG, "IME: beginBatchEdit: mBatchEditCount " + mBatchEditCount
                                                     + " -> " + (mBatchEditCount+1));
        GeckoApp.assertOnUiThread();
        return super.beginBatchEdit();
    }

    @Override
    public boolean endBatchEdit() {
        Log.d(LOGTAG, "IME: endBatchEdit: mBatchEditCount " + mBatchEditCount
                                                   + " -> " + (mBatchEditCount-1));
        GeckoApp.assertOnUiThread();
        if (mBatchEditCount <= 0) {
            throw new IllegalStateException("Expected positive mBatchEditCount, but got "
                                            + mBatchEditCount);
        }
        return super.endBatchEdit();
    }

    @Override
    public boolean commitCompletion(CompletionInfo text) {
        Log.d(LOGTAG, "IME: commitCompletion");
        GeckoApp.assertOnUiThread();
        return super.commitCompletion(text);
    }

    @Override
    public boolean commitText(CharSequence text, int newCursorPosition) {
        Log.d(LOGTAG, String.format("IME: commitText(\"%s\", %d)", text, newCursorPosition));
        GeckoApp.assertOnUiThread();
        return super.commitText(text, newCursorPosition);
    }

    @Override
    public boolean deleteSurroundingText(int leftLength, int rightLength) {
        Log.d(LOGTAG, "IME: deleteSurroundingText(leftLen=" + leftLength + ", rightLen="
                      + rightLength + ")");
        GeckoApp.assertOnUiThread();
        return super.deleteSurroundingText(leftLength, rightLength);
    }

    @Override
    public boolean finishComposingText() {
        Log.d(LOGTAG, "IME: finishComposingText");
        // finishComposingText will post itself to the ui thread,
        // no need to assert it.
        return super.finishComposingText();
    }

    @Override
    public Editable getEditable() {
        Editable editable = super.getEditable();
        Log.d(LOGTAG, "IME: getEditable -> " + prettyPrintString(editable));
        // FIXME: Uncomment assert after bug 780543 is fixed. //GeckoApp.assertOnUiThread();
        return editable;
    }

    @Override
    public boolean performContextMenuAction(int id) {
        Log.d(LOGTAG, "IME: performContextMenuAction");
        GeckoApp.assertOnUiThread();
        return super.performContextMenuAction(id);
    }

    @Override
    public ExtractedText getExtractedText(ExtractedTextRequest req, int flags) {
        Log.d(LOGTAG, "IME: getExtractedText");
        GeckoApp.assertOnUiThread();
        ExtractedText extract = super.getExtractedText(req, flags);
        if (extract != null)
            Log.d(LOGTAG, String.format(
                          ". . . getExtractedText: extract.text=\"%s\", selStart=%d, selEnd=%d",
                          extract.text, extract.selectionStart, extract.selectionEnd));
        return extract;
    }

    @Override
    public CharSequence getTextAfterCursor(int length, int flags) {
        Log.d(LOGTAG, "IME: getTextAfterCursor(length=" + length + ", flags=" + flags + ")");
        GeckoApp.assertOnUiThread();
        CharSequence s = super.getTextAfterCursor(length, flags);
        Log.d(LOGTAG, ". . . getTextAfterCursor returns \"" + s + "\"");
        return s;
    }

    @Override
    public CharSequence getTextBeforeCursor(int length, int flags) {
        Log.d(LOGTAG, "IME: getTextBeforeCursor");
        GeckoApp.assertOnUiThread();
        CharSequence s = super.getTextBeforeCursor(length, flags);
        Log.d(LOGTAG, ". . . getTextBeforeCursor returns \"" + s + "\"");
        return s;
    }

    @Override
    public boolean setComposingText(CharSequence text, int newCursorPosition) {
        Log.d(LOGTAG, String.format("IME: setComposingText(\"%s\", %d)", text, newCursorPosition));
        GeckoApp.assertOnUiThread();
        return super.setComposingText(text, newCursorPosition);
    }

    @Override
    public boolean setComposingRegion(int start, int end) {
        Log.d(LOGTAG, "IME: setComposingRegion(start=" + start + ", end=" + end + ")");
        GeckoApp.assertOnUiThread();
        return super.setComposingRegion(start, end);
    }

    @Override
    public boolean setSelection(int start, int end) {
        Log.d(LOGTAG, "IME: setSelection(start=" + start + ", end=" + end + ")");
        GeckoApp.assertOnUiThread();
        return super.setSelection(start, end);
    }

    @Override
    public String getComposingText() {
        Log.d(LOGTAG, "IME: getComposingText");
        GeckoApp.assertOnUiThread();
        String s = super.getComposingText();
        Log.d(LOGTAG, ". . . getComposingText: Composing text = \"" + s + "\"");
        return s;
    }

    @Override
    public boolean onKeyDel() {
        Log.d(LOGTAG, "IME: onKeyDel");
        GeckoApp.assertOnUiThread();
        return super.onKeyDel();
    }

    @Override
    protected void notifyTextChange(String text, int start, int oldEnd, int newEnd) {
        // notifyTextChange() call is posted to UI thread from notifyIMEChange().
        GeckoApp.assertOnUiThread();
        String msg = String.format("IME: >notifyTextChange(%s, start=%d, oldEnd=%d, newEnd=%d)",
                                   prettyPrintString(text), start, oldEnd, newEnd);
        Log.d(LOGTAG, msg);
        if (start < 0 || oldEnd < start || newEnd < start || newEnd > text.length()) {
            throw new IllegalArgumentException("BUG! " + msg);
        }
        super.notifyTextChange(text, start, oldEnd, newEnd);
    }

    @Override
    protected void notifySelectionChange(int start, int end) {
        // notifySelectionChange() call is posted to UI thread from notifyIMEChange().
        // FIXME: Uncomment assert after bug 780543 is fixed.
        //GeckoApp.assertOnUiThread();
        Log.d(LOGTAG, String.format("IME: >notifySelectionChange(start=%d, end=%d)", start, end));
        super.notifySelectionChange(start, end);
    }

    @Override
    protected void resetCompositionState() {
        Log.d(LOGTAG, "IME: resetCompositionState");
        GeckoApp.assertOnUiThread();
        if (hasCompositionString()) {
            Log.d(LOGTAG, "resetCompositionState() is abandoning an active composition string");
        }
        super.resetCompositionState();
    }

    @Override
    public void onTextChanged(CharSequence s, int start, int before, int count) {
        Log.d(LOGTAG, String.format("IME: onTextChanged(\"%s\" start=%d, before=%d, count=%d)",
                                    s, start, before, count));
        GeckoApp.assertOnUiThread();
        super.onTextChanged(s, start, before, count);
    }

    @Override
    public void afterTextChanged(Editable s) {
        Log.d(LOGTAG, "IME: afterTextChanged(\"" + s + "\")");
        GeckoApp.assertOnUiThread();
        super.afterTextChanged(s);
    }

    @Override
    public void beforeTextChanged(CharSequence s, int start, int count, int after) {
        Log.d(LOGTAG, String.format("IME: beforeTextChanged(\"%s\", start=%d, count=%d, after=%d)",
                                    s, start, count, after));
        GeckoApp.assertOnUiThread();
        super.beforeTextChanged(s, start, count, after);
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        Log.d(LOGTAG, "IME: onCreateInputConnection called");
        GeckoApp.assertOnUiThread();
        return super.onCreateInputConnection(outAttrs);
    }

    @Override
    public boolean onKeyPreIme(int keyCode, KeyEvent event) {
        Log.d(LOGTAG, "IME: onKeyPreIme(keyCode=" + keyCode + ", event=" + event + ")");
        GeckoApp.assertOnUiThread();
        return super.onKeyPreIme(keyCode, event);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        Log.d(LOGTAG, "IME: onKeyDown(keyCode=" + keyCode + ", event=" + event + ")");
        GeckoApp.assertOnUiThread();
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        Log.d(LOGTAG, "IME: onKeyUp(keyCode=" + keyCode + ", event=" + event + ")");
        GeckoApp.assertOnUiThread();
        return super.onKeyUp(keyCode, event);
    }

    @Override
    public boolean onKeyMultiple(int keyCode, int repeatCount, KeyEvent event) {
        Log.d(LOGTAG, "IME: onKeyMultiple(keyCode=" + keyCode + ", repeatCount=" + repeatCount
                      + ", event=" + event + ")");
        GeckoApp.assertOnUiThread();
        return super.onKeyMultiple(keyCode, repeatCount, event);
    }

    @Override
    public boolean onKeyLongPress(int keyCode, KeyEvent event) {
        Log.d(LOGTAG, "IME: onKeyLongPress(keyCode=" + keyCode + ", event=" + event + ")");
        GeckoApp.assertOnUiThread();
        return super.onKeyLongPress(keyCode, event);
    }

    @Override
    public void notifyIME(int type, int state) {
        Log.d(LOGTAG, "IME: >notifyIME(type=" + type + ", state=" + state + ")");
        GeckoApp.assertOnGeckoThread();
        super.notifyIME(type, state);
    }

    @Override
    public void notifyIMEEnabled(int state, String typeHint, String modeHint, String actionHint) {
        Log.d(LOGTAG, "IME: >notifyIMEEnabled(state=" + state + ", typeHint=\"" + typeHint
                      + "\", modeHint=\"" + modeHint + "\", actionHint=\""
                      + actionHint + "\"");
        GeckoApp.assertOnGeckoThread();
        if (state < IME_STATE_DISABLED || state > IME_STATE_PLUGIN)
            throw new IllegalArgumentException("Unexpected IMEState=" + state);
        super.notifyIMEEnabled(state, typeHint, modeHint, actionHint);
    }
}

}
