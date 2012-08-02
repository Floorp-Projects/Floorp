/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.gfx.InputConnectionHandler;
import org.mozilla.gecko.gfx.LayerController;

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
    private static String mIMETypeHint;
    private static String mIMEActionHint;

    private String mCurrentInputMethod;

    // Is a composition active?
    private int mCompositionStart = NO_COMPOSITION_STRING;
    private boolean mCommittingText;
    private KeyCharacterMap mKeyCharacterMap;
    private Editable mEditable;
    private Editable.Factory mEditableFactory;
    private boolean mBatchMode;
    private ExtractedTextRequest mUpdateRequest;
    private final ExtractedText mUpdateExtract = new ExtractedText();

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
        if (mCommittingText)
            Log.e(LOGTAG, "Please report this bug:",
                  new IllegalStateException("commitText, but already committing text?!"));

        mCommittingText = true;
        replaceText(text, newCursorPosition, false);
        mCommittingText = false;

        if (hasCompositionString()) {
            if (DEBUG) Log.d(LOGTAG, ". . . commitText: endComposition");
            endComposition();
        }
        return true;
    }

    @Override
    public boolean finishComposingText() {
        // finishComposingText() is sometimes called even when we are not composing text.
        if (hasCompositionString()) {
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
        Span selection = getSelection();

        switch (id) {
            case R.id.selectAll:
                setSelection(0, text.length());
                break;
            case R.id.cut:
                // Fill the clipboard
                GeckoAppShell.setClipboardText(text);
                // If selection is empty, we'll select everything
                if (selection.length == 0)
                    GeckoAppShell.sendEventToGecko(
                        GeckoEvent.createIMEEvent(GeckoEvent.IME_SET_SELECTION, 0, text.length()));
                GeckoAppShell.sendEventToGecko(
                    GeckoEvent.createIMEEvent(GeckoEvent.IME_DELETE_TEXT, 0, 0));
                break;
            case R.id.paste:
                commitText(GeckoAppShell.getClipboardText(), 1);
                break;
            case R.id.copy:
                // Copy the current selection or the empty string if nothing is selected.
                String copiedText = selection.length > 0
                                    ? text.substring(selection.start, selection.end)
                                    : "";
                GeckoAppShell.setClipboardText(text);
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

        Span selection = getSelection();

        ExtractedText extract = new ExtractedText();
        extract.flags = 0;
        extract.partialStartOffset = -1;
        extract.partialEndOffset = -1;
        extract.selectionStart = selection.start;
        extract.selectionEnd = selection.end;
        extract.startOffset = 0;
        extract.text = content.toString();

        return extract;
    }

    @Override
    public boolean setSelection(int start, int end) {
        // Some IMEs call setSelection() with negative or stale indexes, so clamp them.
        Span newSelection = Span.clamp(start, end, getEditable());
        GeckoAppShell.sendEventToGecko(GeckoEvent.createIMEEvent(GeckoEvent.IME_SET_SELECTION,
                                                                 newSelection.start,
                                                                 newSelection.length));
        return super.setSelection(newSelection.start, newSelection.end);
    }

    @Override
    public CharSequence getTextBeforeCursor(int length, int flags) {
        // Avoid underrunning text buffer.
        Span selection = getSelection();
        if (length > selection.start) {
            length = selection.start;
        }

        if (length < 1) {
            return "";
        }

        return super.getTextBeforeCursor(length, flags);
    }

    @Override
    public CharSequence getTextAfterCursor(int length, int flags) {
        // Avoid overrunning text buffer.
        Span selection = getSelection();
        int contentLength = getEditable().length();
        if (selection.end + length > contentLength) {
            length = contentLength - selection.end;
        }

        if (length < 1) {
            return "";
        }

        return super.getTextAfterCursor(length, flags);
    }

    @Override
    public boolean setComposingText(CharSequence text, int newCursorPosition) {
        // setComposingText() places the given text into the editable, replacing any existing
        // composing text. This method will likely be called multiple times while we are composing
        // text.

        // If the replacement composition string is empty and we have no active composition string
        // to replace, then just ignore the empty string. Some VKBs, such as TouchPal Keyboard,
        // send us empty strings at inopportune times, deleting committed text. See bug 768106.
        if (text.length() == 0 && !hasCompositionString())
            return true;

        return super.setComposingText(text, newCursorPosition);
    }

    private static View getView() {
        LayerController controller = GeckoApp.mAppContext.getLayerController();
        return (controller == null ? null : controller.getView());
    }

    private Span getSelection() {
        Editable content = getEditable();
        int start = Selection.getSelectionStart(content);
        int end = Selection.getSelectionEnd(content);
        return Span.clamp(start, end, content);
    }

    private void replaceText(CharSequence text, int newCursorPosition, boolean composing) {
        if (DEBUG) {
            Log.d(LOGTAG, String.format("IME: replaceText(\"%s\", %d, %b)",
                                        text, newCursorPosition, composing));
            GeckoApp.assertOnUiThread();
        }

        if (text == null)
            text = "";

        final Editable content = getEditable();
        if (content == null) {
            return;
        }

        beginBatchEdit();

        // delete composing text set previously.
        int a;
        int b;

        Span composingSpan = getComposingSpan();
        if (composingSpan != null) {
            removeComposingSpans(content);
            a = composingSpan.start;
            b = composingSpan.end;
            composingSpan = null;
        } else {
            Span selection = getSelection();
            a = selection.start;
            b = selection.end;
        }

        if (composing) {
            Spannable sp = null;
            if (!(text instanceof Spannable)) {
                sp = new SpannableStringBuilder(text);
                text = sp;
                // Underline the active composition string.
                sp.setSpan(new UnderlineSpan(), 0, sp.length(),
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
        if (hasCompositionString()) {
            if (DEBUG) Log.d(LOGTAG, ". . . setComposingRegion: endComposition");
            endComposition();
        }

        Span newComposingRegion = Span.clamp(start, end, getEditable());
        return super.setComposingRegion(newComposingRegion.start, newComposingRegion.end);
    }

    public String getComposingText() {
        final Editable content = getEditable();
        if (content == null) {
            return null;
        }

        Span composingSpan = getComposingSpan();
        if (composingSpan == null || composingSpan.length == 0) {
            return "";
        }

        return TextUtils.substring(content, composingSpan.start, composingSpan.end);
    }

    public boolean onKeyDel() {
        // Some IMEs don't update us on deletions
        // In that case we are not updated when a composition
        // is destroyed, and Bad Things happen

        if (!hasCompositionString())
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
        View view = getView();
        if (view == null) {
            return null;
        }
        Context context = view.getContext();
        return InputMethods.getInputMethodManager(context);
    }

    protected void notifyTextChange(InputMethodManager imm, String text,
                                    int start, int oldEnd, int newEnd) {
        if (!mBatchMode) {
            if (!text.contentEquals(mEditable)) {
                if (DEBUG) Log.d(LOGTAG, ". . . notifyTextChange: current mEditable="
                                         + prettyPrintString(mEditable));

                // Editable will be updated by IME event
                if (!hasCompositionString())
                    setEditable(text);
            }
        }

        if (mUpdateRequest == null)
            return;

        View v = getView();

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

        String updatedText = (newEnd > text.length() ? text : text.substring(0, newEnd));
        int updatedTextLength = updatedText.length();

        // Faster to not query for selection
        mUpdateExtract.selectionStart = updatedTextLength;
        mUpdateExtract.selectionEnd = updatedTextLength;

        mUpdateExtract.text = updatedText;
        mUpdateExtract.startOffset = 0;

        imm.updateExtractedText(v, mUpdateRequest.token, mUpdateExtract);
    }

    protected void notifySelectionChange(InputMethodManager imm, int start, int end) {
        if (!mBatchMode) {
            final Editable content = getEditable();

            Span newSelection = Span.clamp(start, end, content);
            start = newSelection.start;
            end = newSelection.end;

            Span currentSelection = getSelection();
            int a = currentSelection.start;
            int b = currentSelection.end;

            if (start != a || end != b) {
                if (DEBUG) {
                    Log.d(LOGTAG, String.format(
                          ". . . notifySelectionChange: current editable selection: [%d, %d)",
                          a, b));
                }

                super.setSelection(start, end);

                // Check if the selection is inside composing span
                Span composingSpan = getComposingSpan();
                if (composingSpan != null) {
                    int ca = composingSpan.start;
                    int cb = composingSpan.end;
                    if (start < ca || start > cb || end < ca || end > cb) {
                        if (DEBUG) Log.d(LOGTAG, ". . . notifySelectionChange: removeComposingSpans");
                        removeComposingSpans(content);
                    }
                }
            }
        }

        if (imm != null && imm.isFullscreenMode()) {
            View v = getView();
            if (hasCompositionString()) {
                Span span = getComposingSpan();
                imm.updateSelection(v, start, end, span.start, span.end);
            } else {
                imm.updateSelection(v, start, end, -1, -1);
            }

        }
    }

    protected void resetCompositionState() {
        mCompositionStart = NO_COMPOSITION_STRING;
        mBatchMode = false;
        mUpdateRequest = null;
    }

    // TextWatcher
    public void onTextChanged(CharSequence s, int start, int before, int count) {
        if (hasCompositionString() && mCompositionStart != start) {
            // Changed range is different from the composition, need to reset the composition
            endComposition();
        }

        CharSequence changedText = s.subSequence(start, start + count);
        if (DEBUG) {
            Log.d(LOGTAG, "onTextChanged: changedText=\"" + changedText + "\"");
        }

        if (changedText.length() == 1) {
            char changedChar = changedText.charAt(0);

            // Some IMEs (e.g. SwiftKey X) send a string with '\n' when Enter is pressed
            // Such string cannot be handled by Gecko, so we convert it to a key press instead
            if (changedChar == '\n') {
                processKeyDown(KeyEvent.KEYCODE_ENTER, new KeyEvent(KeyEvent.ACTION_DOWN,
                                                                    KeyEvent.KEYCODE_ENTER), false);
                processKeyUp(KeyEvent.KEYCODE_ENTER, new KeyEvent(KeyEvent.ACTION_UP,
                                                                  KeyEvent.KEYCODE_ENTER), false);
                return;
            }

            // If we are committing a single character and didn't have an active composition string,
            // we can send Gecko keydown/keyup events instead of composition events.
            if (mCommittingText && !hasCompositionString() && sendKeyEventsToGecko(changedChar)) {
                // Block this thread until all pending events are processed
                GeckoAppShell.geckoEventSync();
                return;
            }
        }

        boolean startCompositionString = !hasCompositionString();
        if (startCompositionString) {
            if (DEBUG) Log.d(LOGTAG, ". . . onTextChanged: IME_COMPOSITION_BEGIN");
            GeckoAppShell.sendEventToGecko(
                GeckoEvent.createIMEEvent(GeckoEvent.IME_COMPOSITION_BEGIN, 0, 0));
            mCompositionStart = start;

            if (DEBUG) {
                Log.d(LOGTAG, ". . . onTextChanged: IME_SET_SELECTION, start=" + start + ", len="
                              + before);
            }

            GeckoAppShell.sendEventToGecko(
                GeckoEvent.createIMEEvent(GeckoEvent.IME_SET_SELECTION, start, before));
        }

        sendTextToGecko(changedText, start + count);

        if (DEBUG) {
            Log.d(LOGTAG, ". . . onTextChanged: IME_SET_SELECTION, start=" + (start + count)
                          + ", 0");
        }

        GeckoAppShell.sendEventToGecko(
            GeckoEvent.createIMEEvent(GeckoEvent.IME_SET_SELECTION, start + count, 0));

        // End composition if all characters in the word have been deleted.
        // This fixes autocomplete results not appearing.
        if (count == 0 || (startCompositionString && mCommittingText))
            endComposition();

        // Block this thread until all pending events are processed
        GeckoAppShell.geckoEventSync();
    }

    private boolean sendKeyEventsToGecko(char inputChar) {
        // Synthesize VKB key events that could plausibly generate the input character.
        KeyEvent[] events = synthesizeKeyEvents(inputChar);
        if (events == null) {
            if (DEBUG) {
                Log.d(LOGTAG, "synthesizeKeyEvents: char '" + inputChar
                              + "' has no virtual key mapping");
            }
            return false;
        }

        boolean sentKeyEvents = false;

        for (KeyEvent event : events) {
            if (!KeyEvent.isModifierKey(event.getKeyCode())) {
                if (DEBUG) {
                    Log.d(LOGTAG, "synthesizeKeyEvents: char '" + inputChar
                                  + "' -> action=" + event.getAction()
                                  + ", keyCode=" + event.getKeyCode()
                                  + ", UnicodeChar='" + (char) event.getUnicodeChar() + "'");
                }
                GeckoAppShell.sendEventToGecko(GeckoEvent.createKeyEvent(event));
                sentKeyEvents = true;
            }
        }

        return sentKeyEvents;
    }

    private KeyEvent[] synthesizeKeyEvents(char inputChar) {
        // Some symbol characters produce unusual key events on Froyo and Gingerbread.
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.GINGERBREAD_MR1) {
            switch (inputChar) {
                case '&':
                    // Some Gingerbread devices' KeyCharacterMaps return ALT+7 instead of SHIFT+7,
                    // but some devices like the Droid Bionic treat SHIFT+7 as '7'. So just return
                    // null and onTextChanged() will send "&" as a composition string instead of
                    // KEY_DOWN + KEY_UP event pair. This may break web content listening for '&'
                    // key events, but they will still receive "&" input event.
                    return null;

                case '<':
                case '>':
                    // We can't synthesize KeyEvents for '<' or '>' because Froyo and Gingerbread
                    // return incorrect shifted char codes from KeyEvent.getUnicodeChar().
                    // Send these characters as composition strings, not key events.
                    return null;

                // Some symbol characters produce key events on Froyo and Gingerbread, but not
                // Honeycomb and ICS. Send these characters as composition strings, not key events,
                // to more closely mimic Honeycomb and ICS.
                case UNICODE_BULLET:
                case UNICODE_CENT_SIGN:
                case UNICODE_COPYRIGHT_SIGN:
                case UNICODE_DIVISION_SIGN:
                case UNICODE_DOUBLE_LOW_QUOTATION_MARK:
                case UNICODE_ELLIPSIS:
                case UNICODE_EURO_SIGN:
                case UNICODE_INVERTED_EXCLAMATION_MARK:
                case UNICODE_MULTIPLICATION_SIGN:
                case UNICODE_PI:
                case UNICODE_PILCROW_SIGN:
                case UNICODE_POUND_SIGN:
                case UNICODE_REGISTERED_SIGN:
                case UNICODE_SQUARE_ROOT:
                case UNICODE_TRADEMARK_SIGN:
                case UNICODE_WHITE_BULLET:
                case UNICODE_YEN_SIGN:
                    return null;

                default:
                    // Look up the character's key events in KeyCharacterMap below.
                    break;
            }
        }

        if (mKeyCharacterMap == null) {
            mKeyCharacterMap = KeyCharacterMap.load(KeyCharacterMap.VIRTUAL_KEYBOARD);
        }

        char[] inputChars = { inputChar };
        return mKeyCharacterMap.getEvents(inputChars);
    }

    private static KeyEvent[] createKeyDownKeyUpEvents(int keyCode, int metaState) {
        long now = SystemClock.uptimeMillis();
        KeyEvent keyDown = new KeyEvent(now, now, KeyEvent.ACTION_DOWN, keyCode, 0, metaState);
        KeyEvent keyUp = KeyEvent.changeAction(keyDown, KeyEvent.ACTION_UP);
        KeyEvent[] events = { keyDown, keyUp };
        return events;
    }

    private void endComposition() {
        if (DEBUG) {
            Log.d(LOGTAG, "IME: endComposition: IME_COMPOSITION_END");
            GeckoApp.assertOnUiThread();
        }

        if (!hasCompositionString())
            Log.e(LOGTAG, "Please report this bug:",
                  new IllegalStateException("endComposition, but not composing text?!"));

        GeckoAppShell.sendEventToGecko(
            GeckoEvent.createIMEEvent(GeckoEvent.IME_COMPOSITION_END, 0, 0));

        mCompositionStart = NO_COMPOSITION_STRING;
    }

    private void sendTextToGecko(CharSequence text, int caretPos) {
        if (DEBUG) {
            Log.d(LOGTAG, "IME: sendTextToGecko(\"" + text + "\")");
            GeckoApp.assertOnUiThread();
        }

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
            outAttrs.inputType = InputType.TYPE_CLASS_NUMBER
                                 | InputType.TYPE_NUMBER_FLAG_SIGNED
                                 | InputType.TYPE_NUMBER_FLAG_DECIMAL;
        else if (mIMETypeHint.equalsIgnoreCase("datetime") ||
                 mIMETypeHint.equalsIgnoreCase("datetime-local"))
            outAttrs.inputType = InputType.TYPE_CLASS_DATETIME
                                 | InputType.TYPE_DATETIME_VARIATION_NORMAL;
        else if (mIMETypeHint.equalsIgnoreCase("date"))
            outAttrs.inputType = InputType.TYPE_CLASS_DATETIME
                                 | InputType.TYPE_DATETIME_VARIATION_DATE;
        else if (mIMETypeHint.equalsIgnoreCase("time"))
            outAttrs.inputType = InputType.TYPE_CLASS_DATETIME
                                 | InputType.TYPE_DATETIME_VARIATION_TIME;

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

        GeckoApp app = GeckoApp.mAppContext;
        DisplayMetrics metrics = app.getResources().getDisplayMetrics();
        if (Math.min(metrics.widthPixels, metrics.heightPixels) > INLINE_IME_MIN_DISPLAY_SIZE) {
            // prevent showing full-screen keyboard only when the screen is tall enough
            // to show some reasonable amount of the page (see bug 752709)
            outAttrs.imeOptions |= EditorInfo.IME_FLAG_NO_EXTRACT_UI
                                   | EditorInfo.IME_FLAG_NO_FULLSCREEN;
        }

        // onCreateInputConnection() can be called during composition when input focus
        // is restored from a VKB popup window (such as for entering accented characters)
        // back to our IME. We want to commit our active composition string. Bug 756429
        if (hasCompositionString()) {
            endComposition();
        }

        String prevInputMethod = mCurrentInputMethod;
        mCurrentInputMethod = InputMethods.getCurrentInputMethod(app);

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
            GeckoApp.assertOnUiThread();
        }

        if (keyCode > KeyEvent.getMaxKeyCode())
            return false;

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

        View view = getView();
        KeyListener keyListener = TextKeyListener.getInstance();

        // KeyListener returns true if it handled the event for us.
        if (mIMEState == IME_STATE_DISABLED ||
                keyCode == KeyEvent.KEYCODE_ENTER ||
                keyCode == KeyEvent.KEYCODE_DEL ||
                keyCode == KeyEvent.KEYCODE_TAB ||
                (event.getFlags() & KeyEvent.FLAG_SOFT_KEYBOARD) != 0 ||
                !keyListener.onKeyDown(view, mEditable, keyCode, event)) {
            // Make sure selection in Gecko is up-to-date
            Span selection = getSelection();
            GeckoAppShell.sendEventToGecko(GeckoEvent.createIMEEvent(GeckoEvent.IME_SET_SELECTION,
                                                                     selection.start,
                                                                     selection.length));
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
            GeckoApp.assertOnUiThread();
        }

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

        if (isPreIme && mIMEState != IME_STATE_DISABLED &&
            (event.getMetaState() & KeyEvent.META_ALT_ON) != 0)
            // Let active IME process pre-IME key events
            return false;

        View view = getView();
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
        postToUiThread(new Runnable() {
            public void run() {
                View v = getView();
                if (v == null)
                    return;

                switch (type) {
                    case NOTIFY_IME_RESETINPUTSTATE:
                        if (DEBUG) Log.d(LOGTAG, ". . . notifyIME: reset");

                        // Gecko just cancelled the current composition from underneath us,
                        // so abandon our active composition string WITHOUT committing it!
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
                        IMEStateUpdater.resetIME();
                        break;

                    case NOTIFY_IME_FOCUSCHANGE:
                        if (DEBUG) Log.d(LOGTAG, ". . . notifyIME: focus");
                        IMEStateUpdater.resetIME();
                        break;
                }
            }
        });
    }

    public void notifyIMEEnabled(final int state, final String typeHint, final String actionHint) {
        postToUiThread(new Runnable() {
            public void run() {
                View v = getView();
                if (v == null)
                    return;

                /* When IME is 'disabled', IME processing is disabled.
                   In addition, the IME UI is hidden */
                mIMEState = state;
                mIMETypeHint = typeHint;
                mIMEActionHint = actionHint;
                IMEStateUpdater.enableIME();
            }
        });
    }

    public final void notifyIMEChange(final String text, final int start, final int end,
                                      final int newEnd) {
        postToUiThread(new Runnable() {
            public void run() {
                InputMethodManager imm = getInputMethodManager();
                if (imm == null)
                    return;

                if (newEnd < 0)
                    notifySelectionChange(imm, start, end);
                else
                    notifyTextChange(imm, text, start, end, newEnd);
            }
        });
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

    private void setEditable(String contents) {
        mEditable.removeSpan(this);
        mEditable.replace(0, mEditable.length(), contents);
        mEditable.setSpan(this, 0, contents.length(), Spanned.SPAN_INCLUSIVE_INCLUSIVE);
        Selection.setSelection(mEditable, contents.length());
    }

    private void initEditable(String contents) {
        mEditable = mEditableFactory.newEditable(contents);
        mEditable.setSpan(this, 0, contents.length(), Spanned.SPAN_INCLUSIVE_INCLUSIVE);
        Selection.setSelection(mEditable, contents.length());
    }

    protected final boolean hasCompositionString() {
        return mCompositionStart != NO_COMPOSITION_STRING;
    }

    private Span getComposingSpan() {
        Editable content = getEditable();
        int start = getComposingSpanStart(content);
        int end = getComposingSpanEnd(content);

        // Does the editable have a composing span?
        if (start < 0 || end < 0) {
            if (start != -1 || end != -1) {
                throw new IndexOutOfBoundsException("Bad composing span [" + start + "," + end
                                                     + "), contentLength=" + content.length());
            }
            return null;
        }

        return new Span(start, end, content);
    }

    private static String prettyPrintString(CharSequence s) {
        // Quote string and replace newlines with CR arrows.
        return "\"" + s.toString().replace('\n', UNICODE_CRARR) + "\"";
    }

    private static void postToUiThread(Runnable runnable) {
        // postToUiThread() is called by the Gecko and TimerTask threads.
        // The UI thread does not need to post Runnables to itself.
        GeckoApp.mAppContext.mMainHandler.post(runnable);
    }

    private static final class Span {
        public final int start;
        public final int end;
        public final int length;

        public static Span clamp(int start, int end, Editable content) {
            return new Span(start, end, content);
        }

        private Span(int a, int b, Editable content) {
            if (a > b) {
                int tmp = a;
                a = b;
                b = tmp;
            }

            final int contentLength = content.length();

            if (a < 0) {
                a = 0;
            } else if (a > contentLength) {
                a = contentLength;
            }

            if (b < 0) {
                b = 0;
            } else if (b > contentLength) {
                b = contentLength;
            }

            start = a;
            end = b;
            length = end - start;
        }
    }

private static final class DebugGeckoInputConnection extends GeckoInputConnection {
    public DebugGeckoInputConnection(View targetView) {
        super(targetView);
        GeckoApp.assertOnUiThread();
    }

    @Override
    public boolean beginBatchEdit() {
        Log.d(LOGTAG, "IME: beginBatchEdit");
        GeckoApp.assertOnUiThread();
        return super.beginBatchEdit();
    }

    @Override
    public boolean endBatchEdit() {
        Log.d(LOGTAG, "IME: endBatchEdit");
        GeckoApp.assertOnUiThread();
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
        GeckoApp.assertOnUiThread();
        return super.finishComposingText();
    }

    @Override
    public Editable getEditable() {
        Editable editable = super.getEditable();
        Log.d(LOGTAG, "IME: getEditable -> " + prettyPrintString(editable));
        GeckoApp.assertOnUiThread();
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
    protected void notifyTextChange(InputMethodManager imm, String text,
                                    int start, int oldEnd, int newEnd) {
        // notifyTextChange() call is posted to UI thread from notifyIMEChange().
        GeckoApp.assertOnUiThread();
        String msg = String.format("IME: >notifyTextChange(%s, start=%d, oldEnd=%d, newEnd=%d)",
                                   prettyPrintString(text), start, oldEnd, newEnd);
        Log.d(LOGTAG, msg);
        if (start < 0 || oldEnd < start || newEnd < start || newEnd > text.length()) {
            throw new IllegalArgumentException("BUG! " + msg);
        }
        super.notifyTextChange(imm, text, start, oldEnd, newEnd);
    }

    @Override
    protected void notifySelectionChange(InputMethodManager imm, int start, int end) {
        // notifySelectionChange() call is posted to UI thread from notifyIMEChange().
        GeckoApp.assertOnUiThread();
        Log.d(LOGTAG, String.format("IME: >notifySelectionChange(start=%d, end=%d)", start, end));
        super.notifySelectionChange(imm, start, end);
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
        Log.d(LOGTAG, String.format("IME: >notifyIME(type=%d, state=%d)", type, state));
        GeckoApp.assertOnGeckoThread();
        super.notifyIME(type, state);
    }
}

}
