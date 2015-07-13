/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.CustomEditText;
import org.mozilla.gecko.InputMethods;
import org.mozilla.gecko.R;
import org.mozilla.gecko.toolbar.BrowserToolbar.OnCommitListener;
import org.mozilla.gecko.toolbar.BrowserToolbar.OnDismissListener;
import org.mozilla.gecko.toolbar.BrowserToolbar.OnFilterListener;
import org.mozilla.gecko.util.GamepadUtils;
import org.mozilla.gecko.util.StringUtils;
import org.mozilla.gecko.util.HardwareUtils;

import android.content.Context;
import android.graphics.Rect;
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
import android.view.View;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputConnectionWrapper;
import android.view.inputmethod.InputMethodManager;
import android.view.accessibility.AccessibilityEvent;
import android.widget.TextView;

/**
* {@code ToolbarEditText} is the text entry used when the toolbar
* is in edit state. It handles all the necessary input method machinery.
* It's meant to be owned by {@code ToolbarEditLayout}.
*/
public class ToolbarEditText extends CustomEditText
                             implements AutocompleteHandler {

    private static final String LOGTAG = "GeckoToolbarEditText";
    private static final NoCopySpan AUTOCOMPLETE_SPAN = new NoCopySpan.Concrete();

    private final Context mContext;

    private OnCommitListener mCommitListener;
    private OnDismissListener mDismissListener;
    private OnFilterListener mFilterListener;

    private ToolbarPrefs mPrefs;

    // The previous autocomplete result returned to us
    private String mAutoCompleteResult = "";
    // Length of the user-typed portion of the result
    private int mAutoCompletePrefixLength;
    // If text change is due to us setting autocomplete
    private boolean mSettingAutoComplete;
    // Spans used for marking the autocomplete text
    private Object[] mAutoCompleteSpans;
    // Do not process autocomplete result
    private boolean mDiscardAutoCompleteResult;

    public ToolbarEditText(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
    }

    void setOnCommitListener(OnCommitListener listener) {
        mCommitListener = listener;
    }

    void setOnDismissListener(OnDismissListener listener) {
        mDismissListener = listener;
    }

    void setOnFilterListener(OnFilterListener listener) {
        mFilterListener = listener;
    }

    @Override
    public void onAttachedToWindow() {
        setOnKeyListener(new KeyListener());
        setOnKeyPreImeListener(new KeyPreImeListener());
        setOnSelectionChangedListener(new SelectionChangeListener());
        addTextChangedListener(new TextChangeListener());
        // Set an inactive search icon on tablet devices when in editing mode
        updateSearchIcon(false);
    }

    @Override
    public void onFocusChanged(boolean gainFocus, int direction, Rect previouslyFocusedRect) {
        super.onFocusChanged(gainFocus, direction, previouslyFocusedRect);

        // Make search icon inactive when edit toolbar search term isn't a user entered
        // search term
        final boolean isActive = !TextUtils.isEmpty(getText());
        updateSearchIcon(isActive);

        if (gainFocus) {
            resetAutocompleteState();
            return;
        }

        removeAutocomplete(getText());

        final InputMethodManager imm = InputMethods.getInputMethodManager(mContext);
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
        super.setText(text, type);

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

    void setToolbarPrefs(final ToolbarPrefs prefs) {
        mPrefs = prefs;
    }

    /**
     * Update the search icon at the left of the edittext based
     * on its state.
     *
     * @param isActive The state of the edittext. Active is when the initialized
     *         text has changed and is not empty.
     */
    void updateSearchIcon(boolean isActive) {
        if (!HardwareUtils.isTablet()) {
            return;
        }

        // When on tablet show a magnifying glass in editing mode
        if (isActive) {
            setCompoundDrawablesWithIntrinsicBounds(R.drawable.search_icon_active, 0, 0, 0);
        } else {
            setCompoundDrawablesWithIntrinsicBounds(R.drawable.search_icon_inactive, 0, 0, 0);
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
            new BackgroundColorSpan(getHighlightColor())
        };

        mAutoCompleteResult = "";

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
        mAutoCompleteResult = "";

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
    @Override
    public final void onAutocomplete(final String result) {
        // If mDiscardAutoCompleteResult is true, we temporarily disabled
        // autocomplete (due to backspacing, etc.) and we should bail early.
        if (mDiscardAutoCompleteResult) {
            return;
        }

        if (!isEnabled() || result == null) {
            mAutoCompleteResult = "";
            return;
        }

        final Editable text = getText();
        final int textLength = text.length();
        final int resultLength = result.length();
        final int autoCompleteStart = text.getSpanStart(AUTOCOMPLETE_SPAN);
        mAutoCompleteResult = result;

        if (autoCompleteStart > -1) {
            // Autocomplete text already exists; we should replace existing autocomplete text.

            // If the result and the current text don't have the same prefixes,
            // the result is stale and we should wait for the another result to come in.
            if (!TextUtils.regionMatches(result, 0, text, 0, autoCompleteStart)) {
                return;
            }

            beginSettingAutocomplete();

            // Replace the existing autocomplete text with new one.
            // replace() preserves the autocomplete spans that we set before.
            text.replace(autoCompleteStart, textLength, result, autoCompleteStart, resultLength);

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
                    !TextUtils.regionMatches(result, 0, text, 0, textLength)) {
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
            text.append(result, textLength, resultLength);

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

            // Restore selection/composing spans.
            for (int i = 0; i < spans.length; i++) {
                final int spanFlag = spanFlags[i];
                if (spanFlag == 0) {
                    // Skip if the span was ignored before.
                    continue;
                }
                text.setSpan(spans[i], spanStarts[i], spanEnds[i], spanFlag);
            }

            endSettingAutocomplete();
        }
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
                    final InputMethodManager imm = InputMethods.getInputMethodManager(mContext);
                    if (imm != null) {
                        imm.restartInput(ToolbarEditText.this);
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

            if (start < 0 || (start == selStart && start == selEnd)) {
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
        @Override
        public void afterTextChanged(final Editable editable) {
            if (!isEnabled() || mSettingAutoComplete) {
                return;
            }

            final String text = getNonAutocompleteText(editable);
            final int textLength = text.length();
            boolean doAutocomplete = mPrefs.shouldAutocomplete();

            if (StringUtils.isSearchQuery(text, false)) {
                doAutocomplete = false;
            } else if (mAutoCompletePrefixLength > textLength) {
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
            updateSearchIcon(textLength > 0);

            if (mFilterListener != null) {
                mFilterListener.onFilter(text, doAutocomplete ? ToolbarEditText.this : null);
            }
        }

        @Override
        public void beforeTextChanged(CharSequence s, int start, int count,
                                      int after) {
            // do nothing
        }

        @Override
        public void onTextChanged(CharSequence s, int start, int before,
                                  int count) {
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
                // Drop the virtual keyboard.
                clearFocus();
                return true;
            }

            return false;
        }
    }

    private class KeyListener implements View.OnKeyListener {
        @Override
        public boolean onKey(View v, int keyCode, KeyEvent event) {
            if (keyCode == KeyEvent.KEYCODE_ENTER || GamepadUtils.isActionKey(event)) {
                if (event.getAction() != KeyEvent.ACTION_DOWN) {
                    return true;
                }

                if (mCommitListener != null) {
                    mCommitListener.onCommit();
                }

                return true;
            }

            if (GamepadUtils.isBackKey(event)) {
                if (mDismissListener != null) {
                    mDismissListener.onDismiss();
                }

                return true;
            }

            if ((keyCode == KeyEvent.KEYCODE_DEL ||
                (Versions.feature11Plus &&
                 keyCode == KeyEvent.KEYCODE_FORWARD_DEL)) &&
                removeAutocomplete(getText())) {
                // Delete autocomplete text when backspacing or forward deleting.
                return true;
            }

            return false;
        }
    }
}
