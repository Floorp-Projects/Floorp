/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.widget;

import android.content.Context;
import android.graphics.Rect;
import android.os.Build;
import android.support.annotation.NonNull;
import android.support.v4.content.ContextCompat;
import android.text.Editable;
import android.text.NoCopySpan;
import android.text.Selection;
import android.text.Spanned;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.text.style.BackgroundColorSpan;
import android.util.AttributeSet;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputConnectionWrapper;
import android.view.inputmethod.InputMethodManager;
import android.widget.TextView;

import org.mozilla.focus.R;
import org.mozilla.focus.utils.UrlUtils;

public class InlineAutocompleteEditText extends android.support.v7.widget.AppCompatEditText {
    public interface OnCommitListener {
        void onCommit();
    }

    public interface OnFilterListener {
        void onFilter(String searchText, InlineAutocompleteEditText view);
    }

    public interface OnSearchStateChangeListener {
        void onSearchStateChange(boolean isActive);
    }

    public interface OnTextChangeListener {
        void onTextChange(String originalText, String autocompleteText);
    }

    public static class AutocompleteResult {
        public static AutocompleteResult emptyResult() {
            return new AutocompleteResult(null, null);
        }

        public final String text;
        public final String source;

        public AutocompleteResult(String text, String source) {
            this.text = text;
            this.source = source;
        }

        public boolean isEmpty() {
            return text == null;
        }

        public String getText() {
            return text;
        }

        public String getSource() {
            return source;
        }

        public int getLength() {
            return text.length();
        }

        public boolean startsWith(String text) {
            return this.text.startsWith(text);
        }
    }

    private static final String LOGTAG = "GeckoToolbarEditText";
    private static final NoCopySpan AUTOCOMPLETE_SPAN = new NoCopySpan.Concrete();

    private final Context mContext;

    private OnCommitListener mCommitListener;
    private OnFilterListener mFilterListener;
    private OnSearchStateChangeListener mSearchStateChangeListener;
    private OnTextChangeListener mTextChangeListener;

    // The previous autocomplete result returned to us
    private AutocompleteResult mAutoCompleteResult = AutocompleteResult.emptyResult();
    // Length of the user-typed portion of the result
    private int mAutoCompletePrefixLength;
    // If text change is due to us setting autocomplete
    private boolean mSettingAutoComplete;
    // Spans used for marking the autocomplete text
    private Object[] mAutoCompleteSpans;
    // Do not process autocomplete result
    private boolean mDiscardAutoCompleteResult;

    public InlineAutocompleteEditText(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
    }

    public void setOnCommitListener(OnCommitListener listener) {
        mCommitListener = listener;
    }

    public void setOnFilterListener(OnFilterListener listener) {
        mFilterListener = listener;
    }

    void setOnSearchStateChangeListener(OnSearchStateChangeListener listener) {
        mSearchStateChangeListener = listener;
    }

    public void setOnTextChangeListener(OnTextChangeListener listener) {
        mTextChangeListener = listener;
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();
        setOnKeyListener(new KeyListener());
        setOnKeyPreImeListener(new KeyPreImeListener());
        setOnSelectionChangedListener(new SelectionChangeListener());
        addTextChangedListener(new TextChangeListener());
    }

    @Override
    public void onFocusChanged(boolean gainFocus, int direction, Rect previouslyFocusedRect) {
        super.onFocusChanged(gainFocus, direction, previouslyFocusedRect);

        // Make search icon inactive when edit toolbar search term isn't a user entered
        // search term
        final boolean isActive = !TextUtils.isEmpty(getText());
        if (mSearchStateChangeListener != null) {
            mSearchStateChangeListener.onSearchStateChange(isActive);
        }

        if (gainFocus) {
            resetAutocompleteState();
            return;
        }

        removeAutocomplete(getText());

        final InputMethodManager imm = (InputMethodManager) mContext.getSystemService(Context.INPUT_METHOD_SERVICE);
        try {
            imm.restartInput(this);
            imm.hideSoftInputFromWindow(getWindowToken(), 0);
        } catch (NullPointerException e) {
            Log.e(LOGTAG, "InputMethodManagerService, why are you throwing"
                    + " a NullPointerException? See bug 782096", e);
        }
    }

    @Override
    public void setText(final CharSequence text, final TextView.BufferType type) {
        final String textString = (text == null) ? "" : text.toString();

        super.setText(textString, type);

        // Any autocomplete text would have been overwritten, so reset our autocomplete states.
        resetAutocompleteState();
    }

    @Override
    public void sendAccessibilityEventUnchecked(AccessibilityEvent event) {
        // We need to bypass the isShown() check in the default implementation
        // for TYPE_VIEW_TEXT_SELECTION_CHANGED events so that accessibility
        // services could detect a url change.
        if (event.getEventType() == AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED &&
                getParent() != null && !isShown()) {
            onInitializeAccessibilityEvent(event);
            dispatchPopulateAccessibilityEvent(event);
            getParent().requestSendAccessibilityEvent(this, event);
        } else {
            super.sendAccessibilityEventUnchecked(event);
        }
    }

    /**
     * Mark the start of autocomplete changes so our text change
     * listener does not react to changes in autocomplete text
     */
    private void beginSettingAutocomplete() {
        beginBatchEdit();
        mSettingAutoComplete = true;
    }

    /**
     * Mark the end of autocomplete changes
     */
    private void endSettingAutocomplete() {
        mSettingAutoComplete = false;
        endBatchEdit();
    }

    /**
     * Reset autocomplete states to their initial values
     */
    private void resetAutocompleteState() {
        mAutoCompleteSpans = new Object[] {
                // Span to mark the autocomplete text
                AUTOCOMPLETE_SPAN,
                // Span to change the autocomplete text color
                new BackgroundColorSpan(
                        ContextCompat.getColor(getContext(), R.color.colorAccent))
        };

        mAutoCompleteResult = AutocompleteResult.emptyResult();

        // Pretend we already autocompleted the existing text,
        // so that actions like backspacing don't trigger autocompletion.
        mAutoCompletePrefixLength = getText().length();

        // Show the cursor.
        setCursorVisible(true);
    }

    protected String getNonAutocompleteText() {
        return getNonAutocompleteText(getText());
    }

    /**
     * Get the portion of text that is not marked as autocomplete text.
     *
     * @param text Current text content that may include autocomplete text
     */
    private static String getNonAutocompleteText(final Editable text) {
        final int start = text.getSpanStart(AUTOCOMPLETE_SPAN);
        if (start < 0) {
            // No autocomplete text; return the whole string.
            return text.toString();
        }

        // Only return the portion that's not autocomplete text
        return TextUtils.substring(text, 0, start);
    }

    /**
     * Remove any autocomplete text
     *
     * @param text Current text content that may include autocomplete text
     */
    private boolean removeAutocomplete(final Editable text) {
        final int start = text.getSpanStart(AUTOCOMPLETE_SPAN);
        if (start < 0) {
            // No autocomplete text
            return false;
        }

        beginSettingAutocomplete();

        // When we call delete() here, the autocomplete spans we set are removed as well.
        text.delete(start, text.length());

        // Keep mAutoCompletePrefixLength the same because the prefix has not changed.
        // Clear mAutoCompleteResult to make sure we get fresh autocomplete text next time.
        mAutoCompleteResult = AutocompleteResult.emptyResult();

        // Reshow the cursor.
        setCursorVisible(true);

        endSettingAutocomplete();
        return true;
    }

    /**
     * Convert any autocomplete text to regular text
     *
     * @param text Current text content that may include autocomplete text
     */
    private boolean commitAutocomplete(final Editable text) {
        final int start = text.getSpanStart(AUTOCOMPLETE_SPAN);
        if (start < 0) {
            // No autocomplete text
            return false;
        }

        beginSettingAutocomplete();

        // Remove all spans here to convert from autocomplete text to regular text
        for (final Object span : mAutoCompleteSpans) {
            text.removeSpan(span);
        }

        // Keep mAutoCompleteResult the same because the result has not changed.
        // Reset mAutoCompletePrefixLength because the prefix now includes the autocomplete text.
        mAutoCompletePrefixLength = text.length();

        // Reshow the cursor.
        setCursorVisible(true);

        endSettingAutocomplete();

        // Filter on the new text
        if (mFilterListener != null) {
            mFilterListener.onFilter(text.toString(), null);
        }
        return true;
    }

    /**
     * Add autocomplete text based on the result URI.
     *
     * @param result Result URI to be turned into autocomplete text
     */
    public final void onAutocomplete(final AutocompleteResult result) {
        // If mDiscardAutoCompleteResult is true, we temporarily disabled
        // autocomplete (due to backspacing, etc.) and we should bail early.
        if (mDiscardAutoCompleteResult) {
            return;
        }

        if (!isEnabled() || result == null || result.isEmpty()) {
            mAutoCompleteResult = AutocompleteResult.emptyResult();
            return;
        }

        final Editable text = getText();
        final int textLength = text.length();
        final int resultLength = result.getLength();
        final int autoCompleteStart = text.getSpanStart(AUTOCOMPLETE_SPAN);
        mAutoCompleteResult = result;

        if (autoCompleteStart > -1) {
            // Autocomplete text already exists; we should replace existing autocomplete text.

            // If the result and the current text don't have the same prefixes,
            // the result is stale and we should wait for the another result to come in.
            if (!TextUtils.regionMatches(result.getText(), 0, text, 0, autoCompleteStart)) {
                return;
            }

            beginSettingAutocomplete();

            // Replace the existing autocomplete text with new one.
            // replace() preserves the autocomplete spans that we set before.
            text.replace(autoCompleteStart, textLength, result.getText(), autoCompleteStart, resultLength);

            // Reshow the cursor if there is no longer any autocomplete text.
            if (autoCompleteStart == resultLength) {
                setCursorVisible(true);
            }

            endSettingAutocomplete();

        } else {
            // No autocomplete text yet; we should add autocomplete text

            // If the result prefix doesn't match the current text,
            // the result is stale and we should wait for the another result to come in.
            if (resultLength <= textLength ||
                    !TextUtils.regionMatches(result.getText(), 0, text, 0, textLength)) {
                return;
            }

            final Object[] spans = text.getSpans(textLength, textLength, Object.class);
            final int[] spanStarts = new int[spans.length];
            final int[] spanEnds = new int[spans.length];
            final int[] spanFlags = new int[spans.length];

            // Save selection/composing span bounds so we can restore them later.
            for (int i = 0; i < spans.length; i++) {
                final Object span = spans[i];
                final int spanFlag = text.getSpanFlags(span);

                // We don't care about spans that are not selection or composing spans.
                // For those spans, spanFlag[i] will be 0 and we don't restore them.
                if ((spanFlag & Spanned.SPAN_COMPOSING) == 0 &&
                        (span != Selection.SELECTION_START) &&
                        (span != Selection.SELECTION_END)) {
                    continue;
                }

                spanStarts[i] = text.getSpanStart(span);
                spanEnds[i] = text.getSpanEnd(span);
                spanFlags[i] = spanFlag;
            }

            beginSettingAutocomplete();

            // First add trailing text.
            text.append(result.getText(), textLength, resultLength);

            // Restore selection/composing spans.
            for (int i = 0; i < spans.length; i++) {
                final int spanFlag = spanFlags[i];
                if (spanFlag == 0) {
                    // Skip if the span was ignored before.
                    continue;
                }
                text.setSpan(spans[i], spanStarts[i], spanEnds[i], spanFlag);
            }

            // Mark added text as autocomplete text.
            for (final Object span : mAutoCompleteSpans) {
                text.setSpan(span, textLength, resultLength, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
            }

            // Hide the cursor.
            setCursorVisible(false);

            // Make sure the autocomplete text is visible. If the autocomplete text is too
            // long, it would appear the cursor will be scrolled out of view. However, this
            // is not the case in practice, because EditText still makes sure the cursor is
            // still in view.
            bringPointIntoView(resultLength);

            endSettingAutocomplete();
        }

        announceForAccessibility(text.toString());
    }

    public String getOriginalText() {
        final Editable text = getText();

        return text.subSequence(0, mAutoCompletePrefixLength).toString();
    }

    private static boolean hasCompositionString(Editable content) {
        Object[] spans = content.getSpans(0, content.length(), Object.class);

        if (spans != null) {
            for (Object span : spans) {
                if ((content.getSpanFlags(span) & Spanned.SPAN_COMPOSING) != 0) {
                    // Found composition string.
                    return true;
                }
            }
        }

        return false;
    }

    /**
     * Code to handle deleting autocomplete first when backspacing.
     * If there is no autocomplete text, both removeAutocomplete() and commitAutocomplete()
     * are no-ops and return false. Therefore we can use them here without checking explicitly
     * if we have autocomplete text or not.
     *
     * Also turns off text prediction for private mode tabs.
     */
    @Override
    public InputConnection onCreateInputConnection(final EditorInfo outAttrs) {
        final InputConnection ic = super.onCreateInputConnection(outAttrs);
        if (ic == null) {
            return null;
        }

        return new InputConnectionWrapper(ic, false) {
            @Override
            public boolean deleteSurroundingText(final int beforeLength, final int afterLength) {
                if (removeAutocomplete(getText())) {
                    // If we have autocomplete text, the cursor is at the boundary between
                    // regular and autocomplete text. So regardless of which direction we
                    // are deleting, we should delete the autocomplete text first.
                    // Make the IME aware that we interrupted the deleteSurroundingText call,
                    // by restarting the IME.
                    final InputMethodManager imm = (InputMethodManager) mContext.getSystemService(Context.INPUT_METHOD_SERVICE);
                    if (imm != null) {
                        imm.restartInput(InlineAutocompleteEditText.this);
                    }
                    return false;
                }
                return super.deleteSurroundingText(beforeLength, afterLength);
            }

            private boolean removeAutocompleteOnComposing(final CharSequence text) {
                final Editable editable = getText();
                final int composingStart = BaseInputConnection.getComposingSpanStart(editable);
                final int composingEnd = BaseInputConnection.getComposingSpanEnd(editable);
                // We only delete the autocomplete text when the user is backspacing,
                // i.e. when the composing text is getting shorter.
                if (composingStart >= 0 &&
                        composingEnd >= 0 &&
                        (composingEnd - composingStart) > text.length() &&
                        removeAutocomplete(editable)) {
                    // Make the IME aware that we interrupted the setComposingText call,
                    // by having finishComposingText() send change notifications to the IME.
                    finishComposingText();
                    setComposingRegion(composingStart, composingEnd);
                    return true;
                }
                return false;
            }

            @Override
            public boolean commitText(CharSequence text, int newCursorPosition) {
                if (removeAutocompleteOnComposing(text)) {
                    return false;
                }
                return super.commitText(text, newCursorPosition);
            }

            @Override
            public boolean setComposingText(final CharSequence text, final int newCursorPosition) {
                if (removeAutocompleteOnComposing(text)) {
                    return false;
                }
                return super.setComposingText(text, newCursorPosition);
            }
        };
    }

    private class SelectionChangeListener implements OnSelectionChangedListener {
        @Override
        public void onSelectionChanged(final int selStart, final int selEnd) {
            // The user has repositioned the cursor somewhere. We need to adjust
            // the autocomplete text depending on where the new cursor is.

            final Editable text = getText();
            final int start = text.getSpanStart(AUTOCOMPLETE_SPAN);

            if (mSettingAutoComplete || start < 0 || (start == selStart && start == selEnd)) {
                // Do not commit autocomplete text if there is no autocomplete text
                // or if selection is still at start of autocomplete text
                return;
            }

            if (selStart <= start && selEnd <= start) {
                // The cursor is in user-typed text; remove any autocomplete text.
                removeAutocomplete(text);
            } else {
                // The cursor is in the autocomplete text; commit it so it becomes regular text.
                commitAutocomplete(text);
            }
        }
    }

    private class TextChangeListener implements TextWatcher {
        private int textLengthBeforeChange;

        @Override
        public void afterTextChanged(final Editable editable) {
            if (!isEnabled() || mSettingAutoComplete) {
                return;
            }

            final String text = getNonAutocompleteText(editable);
            final int textLength = text.length();
            boolean doAutocomplete = true;

            if (UrlUtils.isSearchQuery(text)) {
                doAutocomplete = false;
            } else if (textLength == textLengthBeforeChange - 1 || textLength == 0) {
                // If you're hitting backspace (the string is getting smaller), don't autocomplete
                doAutocomplete = false;
            }

            mAutoCompletePrefixLength = textLength;

            // If we are not autocompleting, we set mDiscardAutoCompleteResult to true
            // to discard any autocomplete results that are in-flight, and vice versa.
            mDiscardAutoCompleteResult = !doAutocomplete;

            if (doAutocomplete && mAutoCompleteResult.startsWith(text)) {
                // If this text already matches our autocomplete text, autocomplete likely
                // won't change. Just reuse the old autocomplete value.
                onAutocomplete(mAutoCompleteResult);
                doAutocomplete = false;
            } else {
                // Otherwise, remove the old autocomplete text
                // until any new autocomplete text gets added.
                removeAutocomplete(editable);
            }

            // Update search icon with an active state since user is typing
            if (mSearchStateChangeListener != null) {
                mSearchStateChangeListener.onSearchStateChange(textLength > 0);
            }

            if (mFilterListener != null) {
                mFilterListener.onFilter(text, doAutocomplete ? InlineAutocompleteEditText.this : null);
            }

            if (mTextChangeListener != null) {
                mTextChangeListener.onTextChange(text, getText().toString());
            }
        }

        @Override
        public void beforeTextChanged(CharSequence s, int start, int count, int after) {
            textLengthBeforeChange = s.length();
        }

        @Override
        public void onTextChanged(CharSequence s, int start, int before, int count) {
            // do nothing
        }
    }

    private class KeyPreImeListener implements OnKeyPreImeListener {
        @Override
        public boolean onKeyPreIme(View v, int keyCode, KeyEvent event) {
            // We only want to process one event per tap
            if (event.getAction() != KeyEvent.ACTION_DOWN) {
                return false;
            }

            if (keyCode == KeyEvent.KEYCODE_ENTER) {
                // If the edit text has a composition string, don't submit the text yet.
                // ENTER is needed to commit the composition string.
                final Editable content = getText();
                if (!hasCompositionString(content)) {
                    if (mCommitListener != null) {
                        mCommitListener.onCommit();
                    }

                    return true;
                }
            }

            if (keyCode == KeyEvent.KEYCODE_BACK) {
                removeAutocomplete(getText());
                return false;
            }

            return false;
        }
    }

    private class KeyListener implements View.OnKeyListener {
        @Override
        public boolean onKey(View v, int keyCode, KeyEvent event) {
            if (keyCode == KeyEvent.KEYCODE_ENTER) {
                if (event.getAction() != KeyEvent.ACTION_DOWN) {
                    return true;
                }

                if (mCommitListener != null) {
                    mCommitListener.onCommit();
                }

                return true;
            }

            if ((keyCode == KeyEvent.KEYCODE_DEL ||
                    (keyCode == KeyEvent.KEYCODE_FORWARD_DEL)) &&
                    removeAutocomplete(getText())) {
                // Delete autocomplete text when backspacing or forward deleting.
                return true;
            }

            return false;
        }
    }

    private OnKeyPreImeListener mOnKeyPreImeListener;
    private OnSelectionChangedListener mOnSelectionChangedListener;
    private OnWindowFocusChangeListener mOnWindowFocusChangeListener;

    public interface OnKeyPreImeListener {
        public boolean onKeyPreIme(View v, int keyCode, KeyEvent event);
    }

    public void setOnKeyPreImeListener(OnKeyPreImeListener listener) {
        mOnKeyPreImeListener = listener;
    }

    @Override
    public boolean onKeyPreIme(int keyCode, KeyEvent event) {
        if (mOnKeyPreImeListener != null) {
            return mOnKeyPreImeListener.onKeyPreIme(this, keyCode, event);
        }

        return false;
    }

    public interface OnSelectionChangedListener {
        public void onSelectionChanged(int selStart, int selEnd);
    }

    public void setOnSelectionChangedListener(OnSelectionChangedListener listener) {
        mOnSelectionChangedListener = listener;
    }

    @Override
    protected void onSelectionChanged(int selStart, int selEnd) {
        if (mOnSelectionChangedListener != null) {
            mOnSelectionChangedListener.onSelectionChanged(selStart, selEnd);
        }

        super.onSelectionChanged(selStart, selEnd);
    }

    public interface OnWindowFocusChangeListener {
        public void onWindowFocusChanged(boolean hasFocus);
    }

    public void setOnWindowFocusChangeListener(OnWindowFocusChangeListener listener) {
        mOnWindowFocusChangeListener = listener;
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (mOnWindowFocusChangeListener != null) {
            mOnWindowFocusChangeListener.onWindowFocusChanged(hasFocus);
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (android.os.Build.VERSION.SDK_INT == Build.VERSION_CODES.M &&
                event.getActionMasked() == MotionEvent.ACTION_UP) {
            // Android 6 occasionally throws a NullPointerException inside Editor.onTouchEvent()
            // for ACTION_UP when attempting to display (uninitialised) text handles. The Editor
            // and TextView IME interactions are quite complex, so I don't know how to properly
            // work around this issue, but we can at least catch the NPE to prevent crashing
            // the whole app.
            // (Editor tries to make both selection handles visible, but in certain cases they haven't
            // been initialised yet, causing the NPE. It doesn't bother to check the selection handle
            // state, and making some other calls to ensure the handles exist doesn't seem like a
            // clean solution either since I don't understand most of the selection logic. This implementation
            // only seems to exist in Android 6, both Android 5 and 7 have different implementations.)
            try {
                return super.onTouchEvent(event);
            } catch (NullPointerException ignored) {
                // Ignore this (see above) - since we're now in an unknown state let's clear all selection
                // (which is still better than an arbitrary crash that we can't control):
                clearFocus();
                return true;
            }
        } else {
            return super.onTouchEvent(event);

        }
    }
}
