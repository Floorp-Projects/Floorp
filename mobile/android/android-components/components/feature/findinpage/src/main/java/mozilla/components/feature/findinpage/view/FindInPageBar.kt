/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.findinpage.view

import android.content.Context
import android.content.res.ColorStateList
import android.text.Editable
import android.text.TextWatcher
import android.util.AttributeSet
import android.util.TypedValue.COMPLEX_UNIT_PX
import android.widget.EditText
import android.widget.TextView
import androidx.annotation.VisibleForTesting
import androidx.appcompat.widget.AppCompatImageButton
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.view.inputmethod.EditorInfoCompat
import mozilla.components.browser.state.state.content.FindResultState
import mozilla.components.feature.findinpage.R
import mozilla.components.support.ktx.android.view.hideKeyboard
import mozilla.components.support.ktx.android.view.showKeyboard

private const val DEFAULT_VALUE = 0

/**
 * A customizable "Find in page" bar implementing [FindInPageView].
 */
class FindInPageBar @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
) : ConstraintLayout(context, attrs, defStyleAttr), FindInPageView {
    private val styling: FindInPageBarStyling = createStyling(context, attrs, defStyleAttr)

    @VisibleForTesting
    internal val queryEditText: EditText

    @VisibleForTesting
    internal val resultsCountTextView: TextView

    @VisibleForTesting
    internal val resultFormat: String =
        context.getString(R.string.mozac_feature_findindpage_result)

    @VisibleForTesting
    internal val accessibilityFormat: String =
        context.getString(R.string.mozac_feature_findindpage_accessibility_result)

    override var listener: FindInPageView.Listener? = null

    /**
     * Sets/gets private mode.
     *
     * In private mode the IME should not update any personalized data such as typing history and personalized language
     * model based on what the user typed.
     */
    override var private: Boolean
        get() = (queryEditText.imeOptions and EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING) != 0
        set(value) {
            queryEditText.imeOptions = if (value) {
                queryEditText.imeOptions or EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING
            } else {
                queryEditText.imeOptions and (EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING.inv())
            }
        }

    init {
        inflate(getContext(), R.layout.mozac_feature_findinpage_view, this)

        queryEditText = findViewById(R.id.find_in_page_query_text)
        resultsCountTextView = findViewById(R.id.find_in_page_result_text)

        bindQueryEditText()
        bindResultsCountView()
        bindPreviousButton()
        bindNextButton()
        bindCloseButton()
    }

    internal fun onQueryChange(newQuery: String) {
        if (newQuery.isNotBlank()) {
            listener?.onFindAll(newQuery)
        } else {
            resultsCountTextView.text = ""
            listener?.onClearMatches()
        }
    }

    override fun focus() {
        queryEditText.showKeyboard()
    }

    override fun clear() {
        queryEditText.text = null
        queryEditText.clearFocus()
        resultsCountTextView.text = null
        resultsCountTextView.contentDescription = null
    }

    override fun displayResult(result: FindResultState) {
        with(result) {
            val ordinal = if (numberOfMatches > 0) activeMatchOrdinal + 1 else activeMatchOrdinal
            resultsCountTextView.text = String.format(resultFormat, ordinal, numberOfMatches)
            resultsCountTextView.setTextColorIfNotDefaultValue(
                if (numberOfMatches > 0) styling.resultCountTextColor else styling.resultNoMatchesTextColor,
            )
            val accessibilityLabel = String.format(accessibilityFormat, ordinal, numberOfMatches)
            resultsCountTextView.contentDescription = accessibilityLabel
            announceForAccessibility(accessibilityLabel)
        }
    }

    private fun createStyling(
        context: Context,
        attrs: AttributeSet?,
        defStyleAttr: Int,
    ): FindInPageBarStyling {
        val attr = context.obtainStyledAttributes(attrs, R.styleable.FindInPageBar, defStyleAttr, 0)

        with(attr) {
            return FindInPageBarStyling(
                getColor(
                    R.styleable.FindInPageBar_findInPageQueryTextColor,
                    DEFAULT_VALUE,
                ),
                getColor(
                    R.styleable.FindInPageBar_findInPageQueryHintTextColor,
                    DEFAULT_VALUE,
                ),
                getDimensionPixelSize(
                    R.styleable.FindInPageBar_findInPageQueryTextSize,
                    DEFAULT_VALUE,
                ),
                getColor(
                    R.styleable.FindInPageBar_findInPageResultCountTextColor,
                    DEFAULT_VALUE,
                ),
                getColor(
                    R.styleable.FindInPageBar_findInPageNoMatchesTextColor,
                    DEFAULT_VALUE,
                ),
                getDimensionPixelSize(
                    R.styleable.FindInPageBar_findInPageResultCountTextSize,
                    DEFAULT_VALUE,
                ),
                getColorStateList(R.styleable.FindInPageBar_findInPageButtonsTint),
            ).also { recycle() }
        }
    }

    private fun bindNextButton() {
        val nextButton = findViewById<AppCompatImageButton>(R.id.find_in_page_next_btn)
        nextButton.setIconTintIfNotDefaultValue(styling.buttonsTint)
        nextButton.setOnClickListener {
            if (queryEditText.text.isNotEmpty()) {
                listener?.onNextResult()
            }
        }
    }

    private fun bindPreviousButton() {
        val previousButton = findViewById<AppCompatImageButton>(R.id.find_in_page_prev_btn)
        previousButton.setIconTintIfNotDefaultValue(styling.buttonsTint)
        previousButton.setOnClickListener {
            if (queryEditText.text.isNotEmpty()) {
                listener?.onPreviousResult()
            }
        }
    }

    private fun bindCloseButton() {
        val closeButton = findViewById<AppCompatImageButton>(R.id.find_in_page_close_btn)
        closeButton.setIconTintIfNotDefaultValue(styling.buttonsTint)
        closeButton.setOnClickListener {
            clear()
            listener?.onClose()
        }
    }

    private fun bindResultsCountView() {
        resultsCountTextView.setTextSizeIfNotDefaultValue(styling.resultCountTextSize)
        resultsCountTextView.setTextColorIfNotDefaultValue(styling.resultCountTextColor)
    }

    @VisibleForTesting
    internal fun bindQueryEditText() {
        with(queryEditText) {
            setTextSizeIfNotDefaultValue(styling.queryTextSize)
            setTextColorIfNotDefaultValue(styling.queryTextColor)
            setHintTextColorIfNotDefaultValue(styling.queryHintTextColor)

            addTextChangedListener(
                object : TextWatcher {
                    override fun afterTextChanged(s: Editable?) = Unit
                    override fun beforeTextChanged(
                        s: CharSequence?,
                        start: Int,
                        count: Int,
                        after: Int,
                    ) = Unit

                    override fun onTextChanged(
                        newCharacter: CharSequence?,
                        start: Int,
                        before: Int,
                        count: Int,
                    ) {
                        val newQuery = newCharacter?.toString() ?: return
                        onQueryChange(newQuery)
                    }
                },
            )

            onFocusChangeListener = OnFocusChangeListener { _, hasFocus ->
                if (!hasFocus) {
                    this@FindInPageBar.hideKeyboard()
                }
            }
        }
    }

    @VisibleForTesting
    internal fun hideKeyboard() {
        queryEditText.hideKeyboard()
    }
}

internal data class FindInPageBarStyling(
    val queryTextColor: Int,
    val queryHintTextColor: Int,
    val queryTextSize: Int,
    val resultCountTextColor: Int,
    val resultNoMatchesTextColor: Int,
    val resultCountTextSize: Int,
    val buttonsTint: ColorStateList?,
)

private fun TextView.setTextSizeIfNotDefaultValue(newValue: Int) {
    if (newValue != DEFAULT_VALUE) {
        setTextSize(COMPLEX_UNIT_PX, newValue.toFloat())
    }
}

private fun TextView.setTextColorIfNotDefaultValue(newValue: Int) {
    if (newValue != DEFAULT_VALUE) {
        setTextColor(newValue)
    }
}

private fun TextView.setHintTextColorIfNotDefaultValue(newValue: Int) {
    if (newValue != DEFAULT_VALUE) {
        setHintTextColor(newValue)
    }
}

private fun AppCompatImageButton.setIconTintIfNotDefaultValue(newValue: ColorStateList?) {
    val safeValue = newValue ?: return
    imageTintList = safeValue
}
