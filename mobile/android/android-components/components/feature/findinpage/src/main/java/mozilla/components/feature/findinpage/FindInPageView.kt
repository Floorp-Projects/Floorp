/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.findinpage

import android.content.Context
import android.content.res.ColorStateList
import android.support.annotation.VisibleForTesting
import android.support.constraint.ConstraintLayout
import android.support.v7.widget.AppCompatImageButton
import android.text.Editable
import android.text.TextWatcher
import android.util.AttributeSet
import android.util.TypedValue.COMPLEX_UNIT_PX
import android.widget.EditText
import android.widget.TextView
import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.EngineSession
import mozilla.components.support.ktx.android.view.hideKeyboard
import mozilla.components.support.ktx.android.view.showKeyboard

private const val DEFAULT_VALUE = 0

/**
 * A customizable FindInPage UI widget.
 */
@Suppress("TooManyFunctions")
class FindInPageView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : ConstraintLayout(context, attrs, defStyleAttr) {

    /**
     * Callback to be invoked when the close button is pressed.
     */
    var onCloseButtonPressListener: (() -> Unit)? = null

    /**
     * Gives access to the underneath engineSession to start the search and to control the navigation between
     * word matches (Next and previous buttons).
     */
    lateinit var engineSession: EngineSession

    @VisibleForTesting
    internal lateinit var accessibilityFormat: String

    private lateinit var styling: FindInPageViewStyling

    @VisibleForTesting
    internal lateinit var resultsCountTextView: TextView

    @VisibleForTesting
    internal lateinit var queryEditText: EditText

    @VisibleForTesting
    internal lateinit var resultFormat: String

    init {
        inflate(getContext(), R.layout.mozac_feature_findinpage_view, this)
        visibility = GONE
        initFormats()
        initStyling(context, attrs, defStyleAttr)
        bindQueryEditText()
        bindResultsCountView()
        bindPreviousButton()
        bindNextButton()
        bindCloseButton()
    }

    /**
     * Makes the [FindInPageView] widget visible.
     */
    fun show() {
        checkNotNull(engineSession) // without it we can't do any interactions with underneath engine.
        visibility = VISIBLE
        queryEditText.requestFocus()
        queryEditText.showKeyboard()
    }

    /**
     * Returns whether or not the widget is visible.
     *
     * @return true if it is visible, otherwise false.
     */
    fun isActive(): Boolean = visibility == VISIBLE

    internal fun onQueryChange(newQuery: String) {
        if (newQuery.isNotBlank()) {
            engineSession.findAll(newQuery)
        } else {
            resultsCountTextView.text = ""
            engineSession.clearFindMatches()
        }
    }

    internal fun onPreviousButtonClicked() {
        engineSession.findNext(false)
        hideKeyboard()
    }

    internal fun onNextButtonClicked() {
        engineSession.findNext(true)
        hideKeyboard()
    }

    internal fun onCloseButtonClicked() {
        onCloseButtonPressListener?.invoke()
        hideKeyboard()
        queryEditText.text = null
        queryEditText.clearFocus()
        resultsCountTextView.text = null
        resultsCountTextView.contentDescription = null
        visibility = GONE
        engineSession.clearFindMatches()
    }

    internal fun onFindResultReceived(result: Session.FindResult) {
        with(result) {
            if (numberOfMatches > 0) {
                // We don't want the presentation of the activeMatchOrdinal to be zero indexed. So let's
                // increment it by one.
                val ordinal = activeMatchOrdinal + 1
                resultsCountTextView.text = String.format(resultFormat, ordinal, numberOfMatches)
                resultsCountTextView.contentDescription = String.format(accessibilityFormat, ordinal, numberOfMatches)
            } else {
                resultsCountTextView.text = ""
                resultsCountTextView.contentDescription = ""
            }
        }
    }

    private fun initFormats() {
        resultFormat = context.getString(R.string.mozac_feature_findindpage_result)
        accessibilityFormat = context.getString(R.string.mozac_feature_findindpage_accessibility_result)
    }

    private fun initStyling(context: Context, attrs: AttributeSet?, defStyleAttr: Int) {
        val attr = context.obtainStyledAttributes(attrs, R.styleable.FindInPageView, defStyleAttr, 0)
        with(attr) {
            styling = FindInPageViewStyling(
                getColor(R.styleable.FindInPageView_findInPageQueryTextColor, DEFAULT_VALUE),
                getColor(R.styleable.FindInPageView_findInPageQueryHintTextColor, DEFAULT_VALUE),
                getDimensionPixelSize(R.styleable.FindInPageView_findInPageQueryTextSize, DEFAULT_VALUE),
                getColor(R.styleable.FindInPageView_findInPageResultCountTextColor, DEFAULT_VALUE),
                getDimensionPixelSize(R.styleable.FindInPageView_findInPageResultCountTextSize, DEFAULT_VALUE),
                getColorStateList(R.styleable.FindInPageView_findInPageButtonsTint)
            )
            recycle()
        }
    }

    private fun bindNextButton() {
        val nextButton = findViewById<AppCompatImageButton>(R.id.find_in_page_next_btn)
        nextButton.setIconTintIfNotDefaultValue(styling.buttonsTint)
        nextButton.setOnClickListener { onNextButtonClicked() }
    }

    private fun bindPreviousButton() {
        val previousButton = findViewById<AppCompatImageButton>(R.id.find_in_page_prev_btn)
        previousButton.setIconTintIfNotDefaultValue(styling.buttonsTint)
        previousButton.setOnClickListener { onPreviousButtonClicked() }
    }

    private fun bindCloseButton() {
        val closeButton = findViewById<AppCompatImageButton>(R.id.find_in_page_close_btn)
        closeButton.setIconTintIfNotDefaultValue(styling.buttonsTint)
        closeButton.setOnClickListener { onCloseButtonClicked() }
    }

    private fun bindResultsCountView() {
        resultsCountTextView = findViewById(R.id.find_in_page_result_text)
        resultsCountTextView.setTextSizeIfNotDefaultValue(styling.resultCountTextSize)
        resultsCountTextView.setTextColorIfNotDefaultValue(styling.resultCountTextColor)
    }

    private fun bindQueryEditText() {
        queryEditText = findViewById(R.id.find_in_page_query_text)

        with(queryEditText) {
            setTextSizeIfNotDefaultValue(styling.queryTextSize)
            setTextColorIfNotDefaultValue(styling.queryTextColor)
            setHintTextColorIfNotDefaultValue(styling.queryHintTextColor)

            addTextChangedListener(object : TextWatcher {
                override fun afterTextChanged(s: Editable?) = Unit
                override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) = Unit

                override fun onTextChanged(newCharacter: CharSequence?, start: Int, before: Int, count: Int) {
                    val newQuery = newCharacter?.toString() ?: return
                    onQueryChange(newQuery)
                }
            })
        }
    }

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
}

internal data class FindInPageViewStyling(
    val queryTextColor: Int,
    val queryHintTextColor: Int,
    val queryTextSize: Int,
    val resultCountTextColor: Int,
    val resultCountTextSize: Int,
    val buttonsTint: ColorStateList?
)
