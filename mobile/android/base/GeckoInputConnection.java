/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Wu <mwu@mozilla.com>
 *   Alex Pakhotin <alexp@mozilla.com>
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

package org.mozilla.gecko;

import android.R;
import android.content.Context;
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
import android.util.Log;
import android.util.LogPrinter;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.CompletionInfo;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.ExtractedText;
import android.view.inputmethod.ExtractedTextRequest;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;

import org.mozilla.gecko.gfx.InputConnectionHandler;

import java.util.Timer;
import java.util.TimerTask;
import java.util.concurrent.SynchronousQueue;

public class GeckoInputConnection
    extends BaseInputConnection
    implements TextWatcher, InputConnectionHandler {

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

    private static final CharacterStyle COMPOSING_SPAN = new UnderlineSpan();
    private static final Timer mIMETimer = new Timer("GeckoInputConnection Timer");
    private static int mIMEState;
    private static String mIMETypeHint;
    private static String mIMEActionHint;
    private static boolean mIMELandscapeFS;

    // Is a composition active?
    private boolean mComposing;
    private int mCompositionStart = -1;
    private Editable mEditable;
    private Editable.Factory mEditableFactory;
    private boolean mBatchMode;
    private ExtractedTextRequest mUpdateRequest;
    private final ExtractedText mUpdateExtract = new ExtractedText();
    private int mSelectionStart;
    private int mSelectionLength;
    private final SynchronousQueue<String> mQueryResult = new SynchronousQueue<String>();

    public static GeckoInputConnection create(View targetView) {
        if (DEBUG)
            return new DebugGeckoInputConnection(targetView);
        else
            return new GeckoInputConnection(targetView);
    }

    protected GeckoInputConnection(View targetView) {
        super(targetView, true);

        mEditableFactory = Editable.Factory.getInstance();
        initEditable("");
        mIMEState = IME_STATE_DISABLED;
        mIMETypeHint = "";
        mIMEActionHint = "";
    }

    @Override
    public boolean beginBatchEdit() {
        mBatchMode = true;
        return true;
    }

    @Override
    public boolean endBatchEdit() {
        mBatchMode = false;
        return true;
    }

    @Override
    public boolean commitCompletion(CompletionInfo text) {
        return commitText(text.getText(), 1);
    }

    @Override
    public boolean commitText(CharSequence text, int newCursorPosition) {
        replaceText(text, newCursorPosition, false);

        if (mComposing) {
            if (DEBUG) Log.d(LOGTAG, ". . . commitText: endComposition");
            endComposition();
        }
        return true;
    }

    @Override
    public boolean finishComposingText() {
        if (mComposing) {
            if (DEBUG) Log.d(LOGTAG, ". . . finishComposingText: endComposition");
            endComposition();
        }

        final Editable content = getEditable();
        if (content != null) {
            beginBatchEdit();
            removeComposingSpans(content);
            endBatchEdit();
        }
        return true;
    }

    @Override
    public Editable getEditable() {
        return mEditable;
    }

    @Override
    public boolean performContextMenuAction(int id) {
        final Editable content = getEditable();
        if (content == null)
            return false;

        String text = content.toString();

        clampSelection();
        int a = Selection.getSelectionStart(content);
        int b = Selection.getSelectionEnd(content);

        switch (id) {
            case R.id.selectAll:
                setSelection(0, text.length());
                break;
            case R.id.cut:
                // Fill the clipboard
                GeckoAppShell.setClipboardText(text);
                // If selection is empty, we'll select everything
                if (a >= b)
                    GeckoAppShell.sendEventToGecko(
                        GeckoEvent.createIMEEvent(GeckoEvent.IME_SET_SELECTION, 0, text.length()));
                GeckoAppShell.sendEventToGecko(
                    GeckoEvent.createIMEEvent(GeckoEvent.IME_DELETE_TEXT, 0, 0));
                break;
            case R.id.paste:
                commitText(GeckoAppShell.getClipboardText(), 1);
                break;
            case R.id.copy:
                // If there is no selection set, we must be doing "Copy All",
                // otherwise get the selection
                if (a < b)
                    text = text.substring(a, b);
                GeckoAppShell.setClipboardText(text.substring(a, b));
                break;
        }
        return true;
    }

    @Override
    public ExtractedText getExtractedText(ExtractedTextRequest req, int flags) {
        if (req == null)
            return null;

        final Editable content = getEditable();
        if (content == null)
            return null;

        if ((flags & GET_EXTRACTED_TEXT_MONITOR) != 0)
            mUpdateRequest = req;

        ExtractedText extract = new ExtractedText();
        extract.flags = 0;
        extract.partialStartOffset = -1;
        extract.partialEndOffset = -1;

        clampSelection();
        extract.selectionStart = Selection.getSelectionStart(content);
        extract.selectionEnd = Selection.getSelectionEnd(content);
        extract.startOffset = 0;

        try {
            extract.text = content.toString();
        } catch (IndexOutOfBoundsException iob) {
            Log.d(LOGTAG,
                  "IndexOutOfBoundsException thrown from getExtractedText(). start: "
                  + Selection.getSelectionStart(content)
                  + " end: " + Selection.getSelectionEnd(content));
            return null;
        }
        return extract;
    }

    @Override
    public boolean setSelection(int start, int end) {
        GeckoAppShell.sendEventToGecko(
            GeckoEvent.createIMEEvent(GeckoEvent.IME_SET_SELECTION, start, end - start));

        return super.setSelection(start, end);
    }

    @Override
    public boolean deleteSurroundingText(int leftLength, int rightLength) {
        clampSelection();
        return super.deleteSurroundingText(leftLength, rightLength);
    }

    @Override
    public int getCursorCapsMode(int reqModes) {
        clampSelection();
        return super.getCursorCapsMode(reqModes);
    }

    @Override
    public CharSequence getTextBeforeCursor(int length, int flags) {
        clampSelection();
        return super.getTextBeforeCursor(length, flags);
    }

    @Override
    public CharSequence getSelectedText(int flags) {
        clampSelection();
        return super.getSelectedText(flags);
    }

    @Override
    public CharSequence getTextAfterCursor(int length, int flags) {
        clampSelection();
        return super.getTextAfterCursor(length, flags);
    }

    @Override
    public boolean setComposingText(CharSequence text, int newCursorPosition) {
        clampSelection();
        return super.setComposingText(text, newCursorPosition);
    }

    // Android's BaseInputConnection.java is vulnerable to IndexOutOfBoundsExceptions because it
    // does not adequately protect against stale indexes for selections exceeding the content length
    // when the Editable content changes. We must clamp the indexes to be safe.
    private void clampSelection() {
        Editable content = getEditable();
        if (content == null) {
            return;
        }

        final int selectionStart = Selection.getSelectionStart(content);
        final int selectionEnd = Selection.getSelectionEnd(content);

        int a = clampContentIndex(content, selectionStart);
        int b = clampContentIndex(content, selectionEnd);

        if (a > b) {
            int tmp = a;
            a = b;
            b = tmp;
        }

        if (a != selectionStart || b != selectionEnd) {
            Log.e(LOGTAG, "CLAMPING BOGUS SELECTION (" + selectionStart + ", " + selectionEnd
                          + "] -> (" + a + ", " + b + "]", new AssertionError());
            setSelection(a, b);
        }
    }

    private static int clampContentIndex(Editable content, int index) {
        if (index < 0) {
            index = 0;
        } else {
            final int contentLength = content.length();
            if (index > contentLength) {
                index = contentLength;
            }
        }
        return index;
    }

    private void replaceText(CharSequence text, int newCursorPosition, boolean composing) {
        if (DEBUG) {
            Log.d(LOGTAG, String.format("IME: replaceText(\"%s\", %d, %b)",
                                        text, newCursorPosition, composing));
        }

        if (text == null)
            text = "";

        final Editable content = getEditable();
        if (content == null) {
            return;
        }

        beginBatchEdit();

        // delete composing text set previously.
        int a = getComposingSpanStart(content);
        int b = getComposingSpanEnd(content);

        if (DEBUG) Log.d(LOGTAG, "Composing span: " + a + " to " + b);

        if (b < a) {
            int tmp = a;
            a = b;
            b = tmp;
        }

        if (a != -1 && b != -1) {
            removeComposingSpans(content);
        } else {
            clampSelection();
            a = Selection.getSelectionStart(content);
            b = Selection.getSelectionEnd(content);
        }

        if (composing) {
            Spannable sp = null;
            if (!(text instanceof Spannable)) {
                sp = new SpannableStringBuilder(text);
                text = sp;
                sp.setSpan(COMPOSING_SPAN, 0, sp.length(),
                        Spanned.SPAN_EXCLUSIVE_EXCLUSIVE | Spanned.SPAN_COMPOSING);
            } else {
                sp = (Spannable) text;
            }
            setComposingSpans(sp);
        }

        if (DEBUG) Log.d(LOGTAG, "Replacing from " + a + " to " + b + " with \""
                + text + "\", composing=" + composing
                + ", type=" + text.getClass().getCanonicalName());

        if (DEBUG) {
            LogPrinter lp = new LogPrinter(Log.VERBOSE, LOGTAG);
            lp.println("Current text:");
            TextUtils.dumpSpans(content, lp, "  ");
            lp.println("Composing text:");
            TextUtils.dumpSpans(text, lp, "  ");
        }

        // Position the cursor appropriately, so that after replacing the
        // desired range of text it will be located in the correct spot.
        // This allows us to deal with filters performing edits on the text
        // we are providing here.
        if (newCursorPosition > 0) {
            newCursorPosition += b - 1;
        } else {
            newCursorPosition += a;
        }
        if (newCursorPosition < 0) newCursorPosition = 0;
        if (newCursorPosition > content.length())
            newCursorPosition = content.length();
        Selection.setSelection(content, newCursorPosition);

        content.replace(a, b, text);

        if (DEBUG) {
            LogPrinter lp = new LogPrinter(Log.VERBOSE, LOGTAG);
            lp.println("Final text:");
            TextUtils.dumpSpans(content, lp, "  ");
        }

        endBatchEdit();
    }

    @Override
    public boolean setComposingRegion(int start, int end) {
        if (mComposing) {
            if (DEBUG) Log.d(LOGTAG, ". . . setComposingRegion: endComposition");
            endComposition();
        }

        return super.setComposingRegion(start, end);
    }

    public String getComposingText() {
        final Editable content = getEditable();
        if (content == null) {
            return null;
        }
        int a = getComposingSpanStart(content);
        int b = getComposingSpanEnd(content);

        if (a < 0 || b < 0)
            return null;

        if (b < a) {
            int tmp = a;
            a = b;
            b = tmp;
        }

        return TextUtils.substring(content, a, b);
    }

    public boolean onKeyDel() {
        // Some IMEs don't update us on deletions
        // In that case we are not updated when a composition
        // is destroyed, and Bad Things happen

        if (!mComposing)
            return false;

        String text = getComposingText();

        if (text != null && text.length() > 1) {
            text = text.substring(0, text.length() - 1);
            replaceText(text, 1, false);
            return false;
        }

        commitText(null, 1);
        return true;
    }

    private static InputMethodManager getInputMethodManager() {
        Context context = GeckoApp.mAppContext.getLayerController().getView().getContext();
        return (InputMethodManager) context.getSystemService(Context.INPUT_METHOD_SERVICE);
    }

    public void notifyTextChange(InputMethodManager imm, String text,
                                 int start, int oldEnd, int newEnd) {
        if (!mBatchMode) {
            if (!text.contentEquals(mEditable)) {
                if (DEBUG) Log.d(LOGTAG, String.format(
                                 ". . . notifyTextChange: current mEditable=\"%s\"",
                                 mEditable.toString()));
                setEditable(text);
            }
        }

        if (mUpdateRequest == null)
            return;

        View v = GeckoApp.mAppContext.getLayerController().getView();

        if (imm == null) {
            imm = getInputMethodManager();
            if (imm == null)
                return;
        }

        mUpdateExtract.flags = 0;

        // We update from (0, oldEnd) to (0, newEnd) because some Android IMEs
        // assume that updates start at zero, according to jchen.
        mUpdateExtract.partialStartOffset = 0;
        mUpdateExtract.partialEndOffset = oldEnd;

        // Faster to not query for selection
        mUpdateExtract.selectionStart = newEnd;
        mUpdateExtract.selectionEnd = newEnd;

        mUpdateExtract.text = text.substring(0, newEnd);
        mUpdateExtract.startOffset = 0;

        imm.updateExtractedText(v, mUpdateRequest.token, mUpdateExtract);
    }

    public void notifySelectionChange(InputMethodManager imm,
                                      int start, int end) {
        if (!mBatchMode) {
            final Editable content = getEditable();

            start = clampContentIndex(content, start);
            end = clampContentIndex(content, end);

            clampSelection();
            int a = Selection.getSelectionStart(content);
            int b = Selection.getSelectionEnd(content);

            if (start != a || end != b) {
                if (DEBUG) {
                    Log.d(LOGTAG, String.format(
                          ". . . notifySelectionChange: current editable selection: [%d, %d]",
                          a, b));
                }

                super.setSelection(start, end);

                // Check if the selection is inside composing span
                int ca = getComposingSpanStart(content);
                int cb = getComposingSpanEnd(content);
                if (cb < ca) {
                    int tmp = ca;
                    ca = cb;
                    cb = tmp;
                }
                if (start < ca || start > cb || end < ca || end > cb) {
                    if (DEBUG) Log.d(LOGTAG, ". . . notifySelectionChange: removeComposingSpans");
                    removeComposingSpans(content);
                }
            }
        }

        if (imm != null && imm.isFullscreenMode()) {
            View v = GeckoApp.mAppContext.getLayerController().getView();
            imm.updateSelection(v, start, end, -1, -1);
        }
    }

    public void reset() {
        mComposing = false;
        mCompositionStart = -1;
        mBatchMode = false;
        mUpdateRequest = null;
    }

    // TextWatcher
    public void onTextChanged(CharSequence s, int start, int before, int count) {
        if (mComposing && mCompositionStart != start) {
            // Changed range is different from the composition, need to reset the composition
            endComposition();
        }

        if (count == 1 && s.charAt(start) == '\n') {
            // Some IMEs (e.g. SwiftKey X) send a string with '\n' when Enter is pressed
            // Such string cannot be handled by Gecko, so we convert it to a key press instead
            if (DEBUG) Log.d(LOGTAG, ". . . onTextChanged: Typed <Enter>");
            processKeyDown(KeyEvent.KEYCODE_ENTER, new KeyEvent(KeyEvent.ACTION_DOWN,
                                                                KeyEvent.KEYCODE_ENTER), false);
            processKeyUp(KeyEvent.KEYCODE_ENTER, new KeyEvent(KeyEvent.ACTION_UP,
                                                              KeyEvent.KEYCODE_ENTER), false);
            return;
        }

        if (!mComposing) {
            if (DEBUG) Log.d(LOGTAG, ". . . onTextChanged: IME_COMPOSITION_BEGIN");
            GeckoAppShell.sendEventToGecko(
                GeckoEvent.createIMEEvent(GeckoEvent.IME_COMPOSITION_BEGIN, 0, 0));
            mComposing = true;
            mCompositionStart = start;

            if (DEBUG) {
                Log.d(LOGTAG, ". . . onTextChanged: IME_SET_SELECTION, start=" + start + ", len="
                              + before);
            }

            GeckoAppShell.sendEventToGecko(
                GeckoEvent.createIMEEvent(GeckoEvent.IME_SET_SELECTION, start, before));
        }

        if (count == 0) {
            if (DEBUG) Log.d(LOGTAG, ". . . onTextChanged: IME_DELETE_TEXT");
            GeckoAppShell.sendEventToGecko(
                GeckoEvent.createIMEEvent(GeckoEvent.IME_DELETE_TEXT, 0, 0));
        } else {
            sendTextToGecko(s.subSequence(start, start + count), start + count);
        }

        if (DEBUG) {
            Log.d(LOGTAG, ". . . onTextChanged: IME_SET_SELECTION, start=" + (start + count)
                          + ", 0");
        }

        GeckoAppShell.sendEventToGecko(
            GeckoEvent.createIMEEvent(GeckoEvent.IME_SET_SELECTION, start + count, 0));

        // Block this thread until all pending events are processed
        GeckoAppShell.geckoEventSync();
    }

    private void endComposition() {
        if (DEBUG) Log.d(LOGTAG, "IME: endComposition: IME_COMPOSITION_END");
        GeckoAppShell.sendEventToGecko(
            GeckoEvent.createIMEEvent(GeckoEvent.IME_COMPOSITION_END, 0, 0));
        mComposing = false;
        mCompositionStart = -1;
    }

    private void sendTextToGecko(CharSequence text, int caretPos) {
        if (DEBUG) Log.d(LOGTAG, "IME: sendTextToGecko(\"" + text + "\")");

        // Handle composition text styles
        if (text != null && text instanceof Spanned) {
            Spanned span = (Spanned) text;
            int spanStart = 0, spanEnd = 0;
            boolean pastSelStart = false, pastSelEnd = false;

            do {
                int rangeType = GeckoEvent.IME_RANGE_CONVERTEDTEXT;
                int rangeStyles = 0, rangeForeColor = 0, rangeBackColor = 0;

                // Find next offset where there is a style transition
                spanEnd = span.nextSpanTransition(spanStart + 1, text.length(),
                    CharacterStyle.class);

                // Empty range, continue
                if (spanEnd <= spanStart)
                    continue;

                // Get and iterate through list of span objects within range
                CharacterStyle[] styles = span.getSpans(spanStart, spanEnd, CharacterStyle.class);

                for (CharacterStyle style : styles) {
                    if (style instanceof UnderlineSpan) {
                        // Text should be underlined
                        rangeStyles |= GeckoEvent.IME_RANGE_UNDERLINE;
                    } else if (style instanceof ForegroundColorSpan) {
                        // Text should be of a different foreground color
                        rangeStyles |= GeckoEvent.IME_RANGE_FORECOLOR;
                        rangeForeColor = ((ForegroundColorSpan) style).getForegroundColor();
                    } else if (style instanceof BackgroundColorSpan) {
                        // Text should be of a different background color
                        rangeStyles |= GeckoEvent.IME_RANGE_BACKCOLOR;
                        rangeBackColor = ((BackgroundColorSpan) style).getBackgroundColor();
                    }
                }

                // Add range to array, the actual styles are
                //  applied when IME_SET_TEXT is sent
                if (DEBUG) {
                    Log.d(LOGTAG, String.format(
                          ". . . sendTextToGecko: IME_ADD_RANGE, %d, %d, %d, %d, %d, %d",
                          spanStart, spanEnd - spanStart, rangeType, rangeStyles, rangeForeColor,
                          rangeBackColor));
                }

                GeckoAppShell.sendEventToGecko(
                    GeckoEvent.createIMERangeEvent(spanStart, spanEnd - spanStart,
                                                  rangeType, rangeStyles,
                                                  rangeForeColor, rangeBackColor));

                spanStart = spanEnd;
            } while (spanStart < text.length());
        } else {
            if (DEBUG) Log.d(LOGTAG, ". . . sendTextToGecko: IME_ADD_RANGE, 0, " + text.length()
                                     + ", IME_RANGE_RAWINPUT, IME_RANGE_UNDERLINE)");
            GeckoAppShell.sendEventToGecko(
                GeckoEvent.createIMERangeEvent(0, text == null ? 0 : text.length(),
                                               GeckoEvent.IME_RANGE_RAWINPUT,
                                               GeckoEvent.IME_RANGE_UNDERLINE, 0, 0));
        }

        // Change composition (treating selection end as where the caret is)
        if (DEBUG) {
            Log.d(LOGTAG, ". . . sendTextToGecko: IME_SET_TEXT, IME_RANGE_CARETPOSITION, \""
                          + text + "\")");
        }

        GeckoAppShell.sendEventToGecko(
            GeckoEvent.createIMERangeEvent(caretPos, 0,
                                           GeckoEvent.IME_RANGE_CARETPOSITION, 0, 0, 0,
                                           text.toString()));
    }

    public void afterTextChanged(Editable s) {
    }

    public void beforeTextChanged(CharSequence s, int start, int count, int after) {
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
            outAttrs.inputType = InputType.TYPE_CLASS_NUMBER;
        else if (mIMETypeHint.equalsIgnoreCase("datetime") ||
                 mIMETypeHint.equalsIgnoreCase("datetime-local"))
            outAttrs.inputType = InputType.TYPE_CLASS_DATETIME |
                                 InputType.TYPE_DATETIME_VARIATION_NORMAL;
        else if (mIMETypeHint.equalsIgnoreCase("date"))
            outAttrs.inputType = InputType.TYPE_CLASS_DATETIME |
                                 InputType.TYPE_DATETIME_VARIATION_DATE;
        else if (mIMETypeHint.equalsIgnoreCase("time"))
            outAttrs.inputType = InputType.TYPE_CLASS_DATETIME |
                                 InputType.TYPE_DATETIME_VARIATION_TIME;

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
        else if (mIMEActionHint != null && mIMEActionHint.length() != 0)
            outAttrs.actionLabel = mIMEActionHint;

        if (mIMELandscapeFS == false)
            outAttrs.imeOptions |= EditorInfo.IME_FLAG_NO_EXTRACT_UI;

        reset();
        return this;
    }

    public boolean onKeyPreIme(int keyCode, KeyEvent event) {
        switch (event.getAction()) {
            case KeyEvent.ACTION_DOWN:
                return processKeyDown(keyCode, event, true);
            case KeyEvent.ACTION_UP:
                return processKeyUp(keyCode, event, true);
            case KeyEvent.ACTION_MULTIPLE:
                return onKeyMultiple(keyCode, event.getRepeatCount(), event);
        }
        return false;
    }

    public boolean onKeyDown(int keyCode, KeyEvent event) {
        return processKeyDown(keyCode, event, false);
    }

    private boolean processKeyDown(int keyCode, KeyEvent event, boolean isPreIme) {
        if (DEBUG) {
            Log.d(LOGTAG, "IME: processKeyDown(keyCode=" + keyCode + ", event=" + event + ", "
                          + isPreIme + ")");
        }

        switch (keyCode) {
            case KeyEvent.KEYCODE_MENU:
            case KeyEvent.KEYCODE_BACK:
            case KeyEvent.KEYCODE_VOLUME_UP:
            case KeyEvent.KEYCODE_VOLUME_DOWN:
            case KeyEvent.KEYCODE_SEARCH:
                return false;
            case KeyEvent.KEYCODE_DEL:
                // See comments in GeckoInputConnection.onKeyDel
                if (onKeyDel()) {
                    return true;
                }
                break;
            case KeyEvent.KEYCODE_ENTER:
                if ((event.getFlags() & KeyEvent.FLAG_EDITOR_ACTION) != 0 &&
                    mIMEActionHint.equalsIgnoreCase("next"))
                    event = new KeyEvent(event.getAction(), KeyEvent.KEYCODE_TAB);
                break;
            default:
                break;
        }

        if (isPreIme && mIMEState != IME_STATE_DISABLED &&
            (event.getMetaState() & KeyEvent.META_ALT_ON) != 0)
            // Let active IME process pre-IME key events
            return false;

        View view = GeckoApp.mAppContext.getLayerController().getView();
        KeyListener keyListener = TextKeyListener.getInstance();

        // KeyListener returns true if it handled the event for us.
        if (mIMEState == IME_STATE_DISABLED ||
                keyCode == KeyEvent.KEYCODE_ENTER ||
                keyCode == KeyEvent.KEYCODE_DEL ||
                keyCode == KeyEvent.KEYCODE_TAB ||
                (event.getFlags() & KeyEvent.FLAG_SOFT_KEYBOARD) != 0 ||
                !keyListener.onKeyDown(view, mEditable, keyCode, event)) {
            // Make sure selection in Gecko is up-to-date
            final Editable content = getEditable();
            clampSelection();
            int a = Selection.getSelectionStart(content);
            int b = Selection.getSelectionEnd(content);
            GeckoAppShell.sendEventToGecko(
                GeckoEvent.createIMEEvent(GeckoEvent.IME_SET_SELECTION, a, b - a));

            GeckoAppShell.sendEventToGecko(GeckoEvent.createKeyEvent(event));
        }
        return true;
    }

    public boolean onKeyUp(int keyCode, KeyEvent event) {
        return processKeyUp(keyCode, event, false);
    }

    private boolean processKeyUp(int keyCode, KeyEvent event, boolean isPreIme) {
        if (DEBUG) {
            Log.d(LOGTAG, "IME: processKeyUp(keyCode=" + keyCode + ", event=" + event + ", "
                          + isPreIme + ")");
        }

        switch (keyCode) {
            case KeyEvent.KEYCODE_BACK:
            case KeyEvent.KEYCODE_SEARCH:
            case KeyEvent.KEYCODE_MENU:
                return false;
            default:
                break;
        }

        if (isPreIme && mIMEState != IME_STATE_DISABLED &&
            (event.getMetaState() & KeyEvent.META_ALT_ON) != 0)
            // Let active IME process pre-IME key events
            return false;

        View view = GeckoApp.mAppContext.getLayerController().getView();
        KeyListener keyListener = TextKeyListener.getInstance();

        if (mIMEState == IME_STATE_DISABLED ||
            keyCode == KeyEvent.KEYCODE_ENTER ||
            keyCode == KeyEvent.KEYCODE_DEL ||
            (event.getFlags() & KeyEvent.FLAG_SOFT_KEYBOARD) != 0 ||
            !keyListener.onKeyUp(view, mEditable, keyCode, event)) {
            GeckoAppShell.sendEventToGecko(GeckoEvent.createKeyEvent(event));
        }

        return true;
    }

    public boolean onKeyMultiple(int keyCode, int repeatCount, KeyEvent event) {
        GeckoAppShell.sendEventToGecko(GeckoEvent.createKeyEvent(event));
        return true;
    }

    public boolean onKeyLongPress(int keyCode, KeyEvent event) {
        View v = GeckoApp.mAppContext.getLayerController().getView();
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

    public void notifyIME(int type, int state) {
        View v = GeckoApp.mAppContext.getLayerController().getView();

        if (v == null)
            return;

        switch (type) {
        case NOTIFY_IME_RESETINPUTSTATE:
            if (DEBUG) Log.d(LOGTAG, ". . . notifyIME: reset");

            // Composition event is already fired from widget.
            // So reset IME flags.
            reset();

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
            IMEStateUpdater.resetIME();
            break;

        case NOTIFY_IME_FOCUSCHANGE:
            if (DEBUG) Log.d(LOGTAG, ". . . notifyIME: focus");
            IMEStateUpdater.resetIME();
            break;
        }
    }

    public void notifyIMEEnabled(int state, String typeHint,
                                 String actionHint, boolean landscapeFS) {
        View v = GeckoApp.mAppContext.getLayerController().getView();

        if (v == null)
            return;

        /* When IME is 'disabled', IME processing is disabled.
           In addition, the IME UI is hidden */
        mIMEState = state;
        mIMETypeHint = typeHint;
        mIMEActionHint = actionHint;
        mIMELandscapeFS = landscapeFS;
        IMEStateUpdater.enableIME();
    }

    public void notifyIMEChange(String text, int start, int end, int newEnd) {
        InputMethodManager imm = getInputMethodManager();
        if (imm == null)
            return;

        if (newEnd < 0)
            notifySelectionChange(imm, start, end);
        else
            notifyTextChange(imm, text, start, end, newEnd);
    }

    public void returnIMEQueryResult(String result, int selectionStart, int selectionLength) {
        mSelectionStart = selectionStart;
        mSelectionLength = selectionLength;
        try {
            mQueryResult.put(result);
        } catch (InterruptedException e) {
        }
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
            if (DEBUG) Log.d(LOGTAG, "IME: run()");
            synchronized (IMEStateUpdater.class) {
                instance = null;
            }

            View v = GeckoApp.mAppContext.getLayerController().getView();
            if (DEBUG) Log.d(LOGTAG, "IME: v=" + v);

            InputMethodManager imm = getInputMethodManager();
            if (imm == null)
                return;

            if (mReset)
                imm.restartInput(v);

            if (!mEnable)
                return;

            if (mIMEState != IME_STATE_DISABLED &&
                mIMEState != IME_STATE_PLUGIN)
                imm.showSoftInput(v, 0);
            else
                imm.hideSoftInputFromWindow(v.getWindowToken(), 0);
        }
    }

    public void setEditable(String contents) {
        mEditable.removeSpan(this);
        mEditable.replace(0, mEditable.length(), contents);
        mEditable.setSpan(this, 0, contents.length(), Spanned.SPAN_INCLUSIVE_INCLUSIVE);
        Selection.setSelection(mEditable, contents.length());
    }

    public void initEditable(String contents) {
        mEditable = mEditableFactory.newEditable(contents);
        mEditable.setSpan(this, 0, contents.length(), Spanned.SPAN_INCLUSIVE_INCLUSIVE);
        Selection.setSelection(mEditable, contents.length());
    }
}

class DebugGeckoInputConnection extends GeckoInputConnection {
    public DebugGeckoInputConnection(View targetView) {
        super(targetView);
    }

    @Override
    public boolean beginBatchEdit() {
        Log.d(LOGTAG, "IME: beginBatchEdit");
        return super.beginBatchEdit();
    }

    @Override
    public boolean endBatchEdit() {
        Log.d(LOGTAG, "IME: endBatchEdit");
        return super.endBatchEdit();
    }

    @Override
    public boolean commitCompletion(CompletionInfo text) {
        Log.d(LOGTAG, "IME: commitCompletion");
        return super.commitCompletion(text);
    }

    @Override
    public boolean commitText(CharSequence text, int newCursorPosition) {
        Log.d(LOGTAG, String.format("IME: commitText(\"%s\", %d)", text, newCursorPosition));
        return super.commitText(text, newCursorPosition);
    }

    @Override
    public boolean deleteSurroundingText(int leftLength, int rightLength) {
        Log.d(LOGTAG, "IME: deleteSurroundingText(leftLen=" + leftLength + ", rightLen="
                      + rightLength + ")");
        return super.deleteSurroundingText(leftLength, rightLength);
    }

    @Override
    public boolean finishComposingText() {
        Log.d(LOGTAG, "IME: finishComposingText");
        return super.finishComposingText();
    }

    @Override
    public Editable getEditable() {
        Editable editable = super.getEditable();
        Log.d(LOGTAG, "IME: getEditable -> " + editable);
        return editable;
    }

    @Override
    public boolean performContextMenuAction(int id) {
        Log.d(LOGTAG, "IME: performContextMenuAction");
        return super.performContextMenuAction(id);
    }

    @Override
    public ExtractedText getExtractedText(ExtractedTextRequest req, int flags) {
        Log.d(LOGTAG, "IME: getExtractedText");
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
        CharSequence s = super.getTextAfterCursor(length, flags);
        Log.d(LOGTAG, ". . . getTextAfterCursor returns \"" + s + "\"");
        return s;
    }

    @Override
    public CharSequence getTextBeforeCursor(int length, int flags) {
        Log.d(LOGTAG, "IME: getTextBeforeCursor");
        CharSequence s = super.getTextBeforeCursor(length, flags);
        Log.d(LOGTAG, ". . . getTextBeforeCursor returns \"" + s + "\"");
        return s;
    }

    @Override
    public boolean setComposingText(CharSequence text, int newCursorPosition) {
        Log.d(LOGTAG, String.format("IME: setComposingText(\"%s\", %d)", text, newCursorPosition));
        return super.setComposingText(text, newCursorPosition);
    }

    @Override
    public boolean setComposingRegion(int start, int end) {
        Log.d(LOGTAG, "IME: setComposingRegion(start=" + start + ", end=" + end + ")");
        return super.setComposingRegion(start, end);
    }

    @Override
    public boolean setSelection(int start, int end) {
        Log.d(LOGTAG, "IME: setSelection(start=" + start + ", end=" + end + ")");
        return super.setSelection(start, end);
    }

    @Override
    public String getComposingText() {
        Log.d(LOGTAG, "IME: getComposingText");
        String s = super.getComposingText();
        Log.d(LOGTAG, ". . . getComposingText: Composing text = \"" + s + "\"");
        return s;
    }

    @Override
    public boolean onKeyDel() {
        Log.d(LOGTAG, "IME: onKeyDel");
        return super.onKeyDel();
    }

    @Override
    public void notifyTextChange(InputMethodManager imm, String text,
                                 int start, int oldEnd, int newEnd) {
        Log.d(LOGTAG, String.format(
                      "IME: >notifyTextChange(\"%s\", start=%d, oldEnd=%d, newEnd=%d)",
                      text, start, oldEnd, newEnd));
        super.notifyTextChange(imm, text, start, oldEnd, newEnd);
    }

    @Override
    public void notifySelectionChange(InputMethodManager imm,
                                      int start, int end) {
        Log.d(LOGTAG, String.format("IME: >notifySelectionChange(start=%d, end=%d)", start, end));
        super.notifySelectionChange(imm, start, end);
    }

    @Override
    public void reset() {
        Log.d(LOGTAG, "IME: reset");
        super.reset();
    }

    @Override
    public void onTextChanged(CharSequence s, int start, int before, int count) {
        Log.d(LOGTAG, String.format("IME: onTextChanged(\"%s\" start=%d, before=%d, count=%d)",
                                    s, start, before, count));
        super.onTextChanged(s, start, before, count);
    }

    @Override
    public void afterTextChanged(Editable s) {
        Log.d(LOGTAG, "IME: afterTextChanged(\"" + s + "\")");
        super.afterTextChanged(s);
    }

    @Override
    public void beforeTextChanged(CharSequence s, int start, int count, int after) {
        Log.d(LOGTAG, String.format("IME: beforeTextChanged(\"%s\", start=%d, count=%d, after=%d)",
                                    s, start, count, after));
        super.beforeTextChanged(s, start, count, after);
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        Log.d(LOGTAG, "IME: onCreateInputConnection called");
        return super.onCreateInputConnection(outAttrs);
    }

    @Override
    public boolean onKeyPreIme(int keyCode, KeyEvent event) {
        Log.d(LOGTAG, "IME: onKeyPreIme(keyCode=" + keyCode + ", event=" + event + ")");
        return super.onKeyPreIme(keyCode, event);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        Log.d(LOGTAG, "IME: onKeyDown(keyCode=" + keyCode + ", event=" + event + ")");
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        Log.d(LOGTAG, "IME: onKeyUp(keyCode=" + keyCode + ", event=" + event + ")");
        return super.onKeyUp(keyCode, event);
    }

    @Override
    public boolean onKeyMultiple(int keyCode, int repeatCount, KeyEvent event) {
        Log.d(LOGTAG, "IME: onKeyMultiple(keyCode=" + keyCode + ", repeatCount=" + repeatCount
                      + ", event=" + event + ")");
        return super.onKeyMultiple(keyCode, repeatCount, event);
    }

    @Override
    public boolean onKeyLongPress(int keyCode, KeyEvent event) {
        Log.d(LOGTAG, "IME: onKeyLongPress(keyCode=" + keyCode + ", event=" + event + ")");
        return super.onKeyLongPress(keyCode, event);
    }

    @Override
    public void notifyIME(int type, int state) {
        Log.d(LOGTAG, String.format("IME: >notifyIME(type=%d, state=%d)", type, state));
        super.notifyIME(type, state);
    }

    @Override
    public void notifyIMEChange(String text, int start, int end, int newEnd) {
        Log.d(LOGTAG, String.format("IME: >notifyIMEChange(\"%s\", start=%d, end=%d, newEnd=%d)",
                                    text, start, end, newEnd));
        super.notifyIMEChange(text, start, end, newEnd);
    }
}
