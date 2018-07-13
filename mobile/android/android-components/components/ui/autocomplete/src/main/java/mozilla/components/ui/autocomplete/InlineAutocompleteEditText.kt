/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ui.autocomplete

import android.content.Context
import android.graphics.Color.parseColor
import android.graphics.Rect
import android.os.Build
import android.support.v7.widget.AppCompatEditText
import android.text.Editable
import android.text.NoCopySpan
import android.text.Selection
import android.text.Spanned
import android.text.TextUtils
import android.text.TextWatcher
import android.text.style.BackgroundColorSpan
import android.util.AttributeSet
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.view.accessibility.AccessibilityEvent
import android.view.inputmethod.BaseInputConnection
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputConnection
import android.view.inputmethod.InputConnectionWrapper
import android.view.inputmethod.InputMethodManager
import android.widget.TextView

typealias OnCommitListener = () -> Unit
typealias OnFilterListener = (String, InlineAutocompleteEditText?) -> Unit
typealias OnSearchStateChangeListener = (Boolean) -> Unit
typealias OnTextChangeListener = (String, String) -> Unit
typealias OnKeyPreImeListener = (View, Int, KeyEvent) -> Boolean
typealias OnSelectionChangedListener = (Int, Int) -> Unit
typealias OnWindowsFocusChangeListener = (Boolean) -> Unit

typealias TextFormatter = (String) -> String

/**
 * A UI edit text component which supports inline autocompletion.
 *
 * The background color of autocomplete spans can be configured using
 * the custom autocompleteBackgroundColor attribute e.g.
 * app:autocompleteBackgroundColor="#ffffff".
 *
 * A filter listener (see [setOnFilterListener]) needs to be attached to
 * provide autocomplete results. It will be invoked when the input
 * text changes. The listener gets direct access to this component (via its view
 * parameter), so it can call {@link applyAutocompleteResult} in return.
 *
 * A commit listener (see [setOnCommitListener]) can be attached which is
 * invoked when the user selected the result i.e. is done editing.
 *
 * Various other listeners can be attached to enhance default behaviour e.g.
 * [setOnSelectionChangedListener] and [setOnWindowsFocusChangeListener] which
 * will be invoked in response to [onSelectionChanged] and [onWindowFocusChanged]
 * respectively (see also [setOnTextChangeListener],
 * [setOnSelectionChangedListener], and [setOnWindowsFocusChangeListener]).
 */
open class InlineAutocompleteEditText @JvmOverloads constructor(
    val ctx: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = R.attr.editTextStyle
) : AppCompatEditText(ctx, attrs, defStyleAttr) {

    data class AutocompleteResult(
        val text: String,
        val source: String,
        val totalItems: Int,
        private val textFormatter: TextFormatter? = null
    ) {
        val isEmpty: Boolean = this.text.isEmpty()
        val length: Int = this.text.length
        val formattedText: String
            get() = textFormatter?.invoke(text) ?: text

        fun startsWith(text: String): Boolean = this.text.startsWith(text)

        companion object {
            fun emptyResult(): AutocompleteResult = AutocompleteResult("", "", 0)
        }
    }

    private var commitListener: OnCommitListener? = null
    fun setOnCommitListener(l: OnCommitListener) { commitListener = l }

    private var filterListener: OnFilterListener? = null
    fun setOnFilterListener(l: OnFilterListener) { filterListener = l }

    private var searchStateChangeListener: OnSearchStateChangeListener? = null
    fun setOnSearchStateChangeListener(l: OnSearchStateChangeListener) { searchStateChangeListener = l }

    private var textChangeListener: OnTextChangeListener? = null
    fun setOnTextChangeListener(l: OnTextChangeListener) { textChangeListener = l }

    private var keyPreImeListener: OnKeyPreImeListener? = null
    fun setOnKeyPreImeListener(l: OnKeyPreImeListener) { keyPreImeListener = l }

    private var selectionChangedListener: OnSelectionChangedListener? = null
    fun setOnSelectionChangedListener(l: OnSelectionChangedListener) { selectionChangedListener = l }

    private var windowFocusChangeListener: OnWindowsFocusChangeListener? = null
    fun setOnWindowsFocusChangeListener(l: OnWindowsFocusChangeListener) { windowFocusChangeListener = l }

    // The previous autocomplete result returned to us
    var autocompleteResult = AutocompleteResult.emptyResult()
        private set

    // Length of the user-typed portion of the result
    private var autoCompletePrefixLength: Int = 0
    // If text change is due to us setting autocomplete
    private var settingAutoComplete: Boolean = false
    // Spans used for marking the autocomplete text
    private var autoCompleteSpans: Array<Any>? = null
    // Do not process autocomplete result
    private var discardAutoCompleteResult: Boolean = false

    val nonAutocompleteText: String
        get() = getNonAutocompleteText(text)

    val originalText: String
        get() = text.subSequence(0, autoCompletePrefixLength).toString()

    private val autoCompleteBackgroundColor: Int = {
        val a = context.obtainStyledAttributes(attrs, R.styleable.InlineAutocompleteEditText)
        val color = a.getColor(R.styleable.InlineAutocompleteEditText_autocompleteBackgroundColor,
                DEFAULT_AUTOCOMPLETE_BACKGROUND_COLOR)
        a.recycle()
        color
    }()

    private val onKeyPreIme = fun (_: View, keyCode: Int, event: KeyEvent): Boolean {
        // We only want to process one event per tap
        if (event.action != KeyEvent.ACTION_DOWN) {
            return false
        }

        if (keyCode == KeyEvent.KEYCODE_ENTER) {
            // If the edit text has a composition string, don't submit the text yet.
            // ENTER is needed to commit the composition string.
            val content = text
            if (!hasCompositionString(content)) {
                commitListener?.invoke()
                return true
            }
        }

        if (keyCode == KeyEvent.KEYCODE_BACK) {
            removeAutocomplete(text)
            return false
        }

        return false
    }

    private val onKey = fun (_: View, keyCode: Int, event: KeyEvent): Boolean {
        if (keyCode == KeyEvent.KEYCODE_ENTER) {
            if (event.action != KeyEvent.ACTION_DOWN) {
                return true
            }

            commitListener?.invoke()
            return true
        }

        // Delete autocomplete text when backspacing or forward deleting.
        return ((keyCode == KeyEvent.KEYCODE_DEL ||
                keyCode == KeyEvent.KEYCODE_FORWARD_DEL) &&
                removeAutocomplete(text))
    }

    private val onSelectionChanged = fun (selStart: Int, selEnd: Int) {
        // The user has repositioned the cursor somewhere. We need to adjust
        // the autocomplete text depending on where the new cursor is.
        val text = text
        val start = text.getSpanStart(AUTOCOMPLETE_SPAN)

        if (settingAutoComplete || start < 0 || start == selStart && start == selEnd) {
            // Do not commit autocomplete text if there is no autocomplete text
            // or if selection is still at start of autocomplete text
            return
        }

        if (selStart <= start && selEnd <= start) {
            // The cursor is in user-typed text; remove any autocomplete text.
            removeAutocomplete(text)
        } else {
            // The cursor is in the autocomplete text; commit it so it becomes regular text.
            commitAutocomplete(text)
        }
    }

    public override fun onAttachedToWindow() {
        super.onAttachedToWindow()

        this.keyPreImeListener = onKeyPreIme
        this.selectionChangedListener = onSelectionChanged

        setOnKeyListener(onKey)
        addTextChangedListener(TextChangeListener())
    }

    public override fun onFocusChanged(gainFocus: Boolean, direction: Int, previouslyFocusedRect: Rect?) {
        super.onFocusChanged(gainFocus, direction, previouslyFocusedRect)

        // Make search icon inactive when edit toolbar search term isn't a user entered
        // search term
        val isActive = !TextUtils.isEmpty(text)

        searchStateChangeListener?.invoke(isActive)

        if (gainFocus) {
            resetAutocompleteState()
            return
        }

        removeAutocomplete(text)

        val imm = ctx.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
        try {
            imm.restartInput(this)
            imm.hideSoftInputFromWindow(windowToken, 0)
        } catch (ignored: NullPointerException) {
            // See bug 782096 for details
        }
    }

    override fun setText(text: CharSequence?, type: TextView.BufferType) {
        val textString = text?.toString() ?: ""
        super.setText(textString, type)

        // Any autocomplete text would have been overwritten, so reset our autocomplete states.
        resetAutocompleteState()
    }

    override fun sendAccessibilityEventUnchecked(event: AccessibilityEvent) {
        // We need to bypass the isShown() check in the default implementation
        // for TYPE_VIEW_TEXT_SELECTION_CHANGED events so that accessibility
        // services could detect a url change.
        if (event.eventType == AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED &&
                parent != null && !isShown) {
            onInitializeAccessibilityEvent(event)
            dispatchPopulateAccessibilityEvent(event)
            parent.requestSendAccessibilityEvent(this, event)
        } else {
            super.sendAccessibilityEventUnchecked(event)
        }
    }

    /**
     * Mark the start of autocomplete changes so our text change
     * listener does not react to changes in autocomplete text
     */
    private fun beginSettingAutocomplete() {
        beginBatchEdit()
        settingAutoComplete = true
    }

    /**
     * Mark the end of autocomplete changes
     */
    private fun endSettingAutocomplete() {
        settingAutoComplete = false
        endBatchEdit()
    }

    /**
     * Reset autocomplete states to their initial values
     */
    private fun resetAutocompleteState() {
        autoCompleteSpans = arrayOf(AUTOCOMPLETE_SPAN, BackgroundColorSpan(autoCompleteBackgroundColor))
        autocompleteResult = AutocompleteResult.emptyResult()
        // Pretend we already autocompleted the existing text,
        // so that actions like backspacing don't trigger autocompletion.
        autoCompletePrefixLength = text.length
        isCursorVisible = true
    }

    /**
     * Remove any autocomplete text
     *
     * @param text Current text content that may include autocomplete text
     */
    private fun removeAutocomplete(text: Editable): Boolean {
        val start = text.getSpanStart(AUTOCOMPLETE_SPAN)
        if (start < 0) {
            // No autocomplete text
            return false
        }

        beginSettingAutocomplete()

        // When we call delete() here, the autocomplete spans we set are removed as well.
        text.delete(start, text.length)

        // Keep autoCompletePrefixLength the same because the prefix has not changed.
        // Clear mAutoCompleteResult to make sure we get fresh autocomplete text next time.
        autocompleteResult = AutocompleteResult.emptyResult()

        // Reshow the cursor.
        isCursorVisible = true

        endSettingAutocomplete()
        return true
    }

    /**
     * Convert any autocomplete text to regular text
     *
     * @param text Current text content that may include autocomplete text
     */
    private fun commitAutocomplete(text: Editable): Boolean {
        val start = text.getSpanStart(AUTOCOMPLETE_SPAN)
        if (start < 0) {
            // No autocomplete text
            return false
        }

        beginSettingAutocomplete()

        // Remove all spans here to convert from autocomplete text to regular text
        for (span in autoCompleteSpans!!) {
            text.removeSpan(span)
        }

        // Keep mAutoCompleteResult the same because the result has not changed.
        // Reset autoCompletePrefixLength because the prefix now includes the autocomplete text.
        autoCompletePrefixLength = text.length

        // Reshow the cursor.
        isCursorVisible = true

        endSettingAutocomplete()

        // Filter on the new text
        filterListener?.invoke(text.toString(), null)
        return true
    }

    /**
     * Applies the provided result by updating the current autocomplete
     * text and selection, if any.
     *
     * @param result the [AutocompleteResult] to apply
     */
    fun applyAutocompleteResult(result: AutocompleteResult) {
        // If discardAutoCompleteResult is true, we temporarily disabled
        // autocomplete (due to backspacing, etc.) and we should bail early.
        if (discardAutoCompleteResult) {
            return
        }

        if (!isEnabled || result.isEmpty) {
            autocompleteResult = AutocompleteResult.emptyResult()
            return
        }

        val text = text
        val textLength = text.length
        val resultLength = result.length
        val autoCompleteStart = text.getSpanStart(AUTOCOMPLETE_SPAN)
        autocompleteResult = result

        if (autoCompleteStart > -1) {
            // Autocomplete text already exists; we should replace existing autocomplete text.

            // If the result and the current text don't have the same prefixes,
            // the result is stale and we should wait for the another result to come in.
            if (!TextUtils.regionMatches(result.text, 0, text, 0, autoCompleteStart)) {
                return
            }

            beginSettingAutocomplete()

            // Replace the existing autocomplete text with new one.
            // replace() preserves the autocomplete spans that we set before.
            text.replace(autoCompleteStart, textLength, result.text, autoCompleteStart, resultLength)

            // Reshow the cursor if there is no longer any autocomplete text.
            if (autoCompleteStart == resultLength) {
                isCursorVisible = true
            }

            endSettingAutocomplete()
        } else {
            // No autocomplete text yet; we should add autocomplete text

            // If the result prefix doesn't match the current text,
            // the result is stale and we should wait for the another result to come in.
            if (resultLength <= textLength || !TextUtils.regionMatches(result.text, 0, text, 0, textLength)) {
                return
            }

            val spans = text.getSpans(textLength, textLength, Any::class.java)
            val spanStarts = IntArray(spans.size)
            val spanEnds = IntArray(spans.size)
            val spanFlags = IntArray(spans.size)

            // Save selection/composing span bounds so we can restore them later.
            for (i in spans.indices) {
                val span = spans[i]
                val spanFlag = text.getSpanFlags(span)

                // We don't care about spans that are not selection or composing spans.
                // For those spans, spanFlag[i] will be 0 and we don't restore them.
                if (spanFlag and Spanned.SPAN_COMPOSING == 0 &&
                        span !== Selection.SELECTION_START &&
                        span !== Selection.SELECTION_END) {
                    continue
                }

                spanStarts[i] = text.getSpanStart(span)
                spanEnds[i] = text.getSpanEnd(span)
                spanFlags[i] = spanFlag
            }

            beginSettingAutocomplete()

            // First add trailing text.
            text.append(result.text, textLength, resultLength)

            // Restore selection/composing spans.
            for (i in spans.indices) {
                val spanFlag = spanFlags[i]
                if (spanFlag == 0) {
                    // Skip if the span was ignored before.
                    continue
                }
                text.setSpan(spans[i], spanStarts[i], spanEnds[i], spanFlag)
            }

            // Mark added text as autocomplete text.
            for (span in autoCompleteSpans!!) {
                text.setSpan(span, textLength, resultLength, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
            }

            // Hide the cursor.
            isCursorVisible = false

            // Make sure the autocomplete text is visible. If the autocomplete text is too
            // long, it would appear the cursor will be scrolled out of view. However, this
            // is not the case in practice, because EditText still makes sure the cursor is
            // still in view.
            bringPointIntoView(resultLength)

            endSettingAutocomplete()
        }

        announceForAccessibility(text.toString())
    }

    /**
     * Code to handle deleting autocomplete first when backspacing.
     * If there is no autocomplete text, both removeAutocomplete() and commitAutocomplete()
     * are no-ops and return false. Therefore we can use them here without checking explicitly
     * if we have autocomplete text or not.
     *
     * Also turns off text prediction for private mode tabs.
     */
    override fun onCreateInputConnection(outAttrs: EditorInfo): InputConnection? {
        val ic = super.onCreateInputConnection(outAttrs) ?: return null

        return object : InputConnectionWrapper(ic, false) {
            override fun deleteSurroundingText(beforeLength: Int, afterLength: Int): Boolean {
                if (removeAutocomplete(text)) {
                    // If we have autocomplete text, the cursor is at the boundary between
                    // regular and autocomplete text. So regardless of which direction we
                    // are deleting, we should delete the autocomplete text first.
                    // Make the IME aware that we interrupted the deleteSurroundingText call,
                    // by restarting the IME.
                    val imm = ctx.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
                    imm.restartInput(this@InlineAutocompleteEditText)
                    return false
                }
                return super.deleteSurroundingText(beforeLength, afterLength)
            }

            private fun removeAutocompleteOnComposing(text: CharSequence): Boolean {
                val editable = getText()
                val composingStart = BaseInputConnection.getComposingSpanStart(editable)
                val composingEnd = BaseInputConnection.getComposingSpanEnd(editable)
                // We only delete the autocomplete text when the user is backspacing,
                // i.e. when the composing text is getting shorter.
                if (composingStart >= 0 &&
                        composingEnd >= 0 &&
                        composingEnd - composingStart > text.length &&
                        removeAutocomplete(editable)) {
                    // Make the IME aware that we interrupted the setComposingText call,
                    // by having finishComposingText() send change notifications to the IME.
                    finishComposingText()
                    return true
                }
                return false
            }

            override fun commitText(text: CharSequence, newCursorPosition: Int): Boolean {
                return if (removeAutocompleteOnComposing(text))
                    false
                else
                    super.commitText(text, newCursorPosition)
            }

            override fun setComposingText(text: CharSequence, newCursorPosition: Int): Boolean {
                return if (removeAutocompleteOnComposing(text))
                    false
                else
                    super.setComposingText(text, newCursorPosition)
            }
        }
    }

    private inner class TextChangeListener : TextWatcher {
        private var textLengthBeforeChange: Int = 0

        override fun afterTextChanged(editable: Editable) {
            if (!isEnabled || settingAutoComplete) {
                return
            }

            val text = getNonAutocompleteText(editable)
            val textLength = text.length
            var doAutocomplete = true

            // Check if search query
            if (text.contains(" ")) {
                doAutocomplete = false
            } else if (textLength == textLengthBeforeChange - 1 || textLength == 0) {
                // If you're hitting backspace (the string is getting smaller), don't autocomplete
                doAutocomplete = false
            }

            autoCompletePrefixLength = textLength

            // If we are not autocompleting, we set discardAutoCompleteResult to true
            // to discard any autocomplete results that are in-flight, and vice versa.
            discardAutoCompleteResult = !doAutocomplete

            if (doAutocomplete && autocompleteResult.startsWith(text)) {
                // If this text already matches our autocomplete text, autocomplete likely
                // won't change. Just reuse the old autocomplete value.
                applyAutocompleteResult(autocompleteResult)
                doAutocomplete = false
            } else {
                // Otherwise, remove the old autocomplete text
                // until any new autocomplete text gets added.
                removeAutocomplete(editable)
            }

            // Update search icon with an active state since user is typing
            searchStateChangeListener?.invoke(textLength > 0)

            filterListener?.invoke(text, if (doAutocomplete) this@InlineAutocompleteEditText else null)

            textChangeListener?.invoke(text, getText().toString())
        }

        override fun beforeTextChanged(s: CharSequence, start: Int, count: Int, after: Int) {
            textLengthBeforeChange = s.length
        }

        override fun onTextChanged(s: CharSequence, start: Int, before: Int, count: Int) {
            // do nothing
        }
    }

    override fun onKeyPreIme(keyCode: Int, event: KeyEvent): Boolean {
        return keyPreImeListener?.invoke(this, keyCode, event) ?: false
    }

    public override fun onSelectionChanged(selStart: Int, selEnd: Int) {
        selectionChangedListener?.invoke(selStart, selEnd)
        super.onSelectionChanged(selStart, selEnd)
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        windowFocusChangeListener?.invoke(hasFocus)
    }

    @Suppress("ClickableViewAccessibility")
    override fun onTouchEvent(event: MotionEvent): Boolean {
        return if (android.os.Build.VERSION.SDK_INT == Build.VERSION_CODES.M &&
                event.actionMasked == MotionEvent.ACTION_UP) {
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
                super.onTouchEvent(event)
            } catch (ignored: NullPointerException) {
                // Ignore this (see above) - since we're now in an unknown state let's clear all selection
                // (which is still better than an arbitrary crash that we can't control):
                clearFocus()
                true
            }
        } else {
            super.onTouchEvent(event)
        }
    }

    companion object {
        val AUTOCOMPLETE_SPAN = NoCopySpan.Concrete()
        val DEFAULT_AUTOCOMPLETE_BACKGROUND_COLOR = parseColor("#ffb5007f")

        /**
         * Get the portion of text that is not marked as autocomplete text.
         *
         * @param text Current text content that may include autocomplete text
         */
        private fun getNonAutocompleteText(text: Editable): String {
            val start = text.getSpanStart(AUTOCOMPLETE_SPAN)
            return if (start < 0) {
                // No autocomplete text; return the whole string.
                text.toString()
            } else {
                // Only return the portion that's not autocomplete text
                TextUtils.substring(text, 0, start)
            }
        }

        private fun hasCompositionString(content: Editable): Boolean {
            val spans = content.getSpans(0, content.length, Any::class.java)
            return spans.any { span -> content.getSpanFlags(span) and Spanned.SPAN_COMPOSING != 0 }
        }
    }
}
