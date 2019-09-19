/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.edit

import android.annotation.SuppressLint
import android.content.Context
import android.text.InputType
import android.view.Gravity
import android.view.KeyEvent
import android.view.View
import android.view.ViewGroup
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputMethodManager
import android.widget.ImageView
import androidx.core.content.ContextCompat
import androidx.core.view.isVisible
import androidx.core.view.setPadding
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.browser.toolbar.R
import mozilla.components.browser.toolbar.facts.emitCommitFact
import mozilla.components.browser.toolbar.internal.ActionWrapper
import mozilla.components.browser.toolbar.internal.invalidateActions
import mozilla.components.browser.toolbar.internal.measureActions
import mozilla.components.browser.toolbar.internal.wrapAction
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.support.ktx.android.view.showKeyboard
import mozilla.components.support.ktx.android.widget.adjustMaxTextSize
import mozilla.components.ui.autocomplete.InlineAutocompleteEditText

/**
 * Sub-component of the browser toolbar responsible for allowing the user to edit the URL.
 *
 * Structure:
 * +---------------------------------+---------+------+
 * |                 url             | actions | exit |
 * +---------------------------------+---------+------+
 *
 * - url: Editable URL of the currently displayed website
 * - actions: Optional action icons injected by other components (e.g. barcode scanner)
 * - exit: Button that switches back to display mode.
 */
@SuppressLint("ViewConstructor") // This view is only instantiated in code
internal class EditToolbar(
    context: Context,
    private val toolbar: BrowserToolbar
) : ViewGroup(context) {
    internal val urlView = InlineAutocompleteEditText(context).apply {
        id = R.id.mozac_browser_toolbar_edit_url_view
        imeOptions = EditorInfo.IME_ACTION_GO or EditorInfo.IME_FLAG_NO_EXTRACT_UI or EditorInfo.IME_FLAG_NO_FULLSCREEN
        gravity = Gravity.CENTER_VERTICAL
        background = null
        textSize = BrowserToolbar.URL_TEXT_SIZE_SP
        inputType = InputType.TYPE_TEXT_VARIATION_URI or InputType.TYPE_CLASS_TEXT
        setSingleLine()

        val horizontalPadding = resources.getDimensionPixelSize(R.dimen.mozac_browser_toolbar_url_horizontal_padding)
        val verticalPadding = resources.getDimensionPixelSize(R.dimen.mozac_browser_toolbar_url_vertical_padding)

        setPadding(horizontalPadding, verticalPadding, horizontalPadding, verticalPadding)

        setOnCommitListener {
            // We emit the fact before notifying the listener because otherwise the listener may cause a focus
            // change which may reset the autocomplete state that we want to report here.
            emitCommitFact(autocompleteResult)

            toolbar.onUrlEntered(text.toString())
        }

        setOnTextChangeListener { text, _ ->
            updateClearViewVisibility(text)
            editListener?.onTextChanged(text)
        }

        setOnDispatchKeyEventPreImeListener { event ->
            if (event?.keyCode == KeyEvent.KEYCODE_BACK) {
                toolbar.onEditCancelled()
            }
            false
        }
    }

    private val defaultColor = ContextCompat.getColor(context, R.color.photonWhite)

    internal var clearViewColor = defaultColor
        set(value) {
            field = value
            clearView.setColorFilter(value)
        }

    internal fun updateClearViewVisibility(text: String) {
        clearView.isVisible = text.isNotBlank()
    }

    private val clearView = ImageView(context).apply {
        visibility = View.GONE
        id = R.id.mozac_browser_toolbar_clear_view
        setPadding(resources.getDimensionPixelSize(R.dimen.mozac_browser_toolbar_cancel_padding))
        setImageResource(mozilla.components.ui.icons.R.drawable.mozac_ic_clear)
        contentDescription = context.getString(R.string.mozac_clear_button_description)
        scaleType = ImageView.ScaleType.CENTER

        setOnClickListener {
            // We set text to an empty string instead of using clear to avoid #3612.
            urlView.setText("")
        }
    }

    internal var editListener: Toolbar.OnEditListener? = null

    private val editActions: MutableList<ActionWrapper> = mutableListOf()

    init {
        addView(urlView)
        addView(clearView)
    }

    /**
     * Updates the URL. This should only be called if the toolbar is not in editing mode. Otherwise
     * this might override the URL the user is currently typing.
     */
    fun updateUrl(url: String, shouldAutoComplete: Boolean = false) {
        urlView.setText(url, shouldAutoComplete)
    }

    /**
     * Focus the URL editing component and show the virtual keyboard if needed.
     */
    fun focus() {
        urlView.showKeyboard(flags = InputMethodManager.SHOW_FORCED)
    }

    /**
     * Adds an action to be displayed on the right of the URL in edit mode.
     */
    fun addEditAction(action: Toolbar.Action) {
        editActions.add(wrapAction(action))
    }

    /**
     * Declare that the actions (navigation actions, browser actions, page actions) have changed and
     * should be updated if needed.
     */
    fun invalidateActions() {
        invalidateActions(editActions)
    }

    // We measure the views manually to avoid overhead by using complex ViewGroup implementations
    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        // This toolbar is using the full size provided by the parent
        val width = MeasureSpec.getSize(widthMeasureSpec)
        val height = MeasureSpec.getSize(heightMeasureSpec)

        setMeasuredDimension(width, height)

        val actionsWidth = measureActions(editActions, height)

        // The icon fills the whole height and has a square shape
        val iconSquareSpec = MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY)
        clearView.measure(iconSquareSpec, iconSquareSpec)

        val urlWidthSpec = MeasureSpec.makeMeasureSpec(
            width - clearView.measuredWidth - actionsWidth,
            MeasureSpec.EXACTLY)

        urlView.measure(urlWidthSpec, heightMeasureSpec)
        urlView.adjustMaxTextSize(heightMeasureSpec)
    }

    // We layout the toolbar ourselves to avoid the overhead from using complex ViewGroup implementations
    override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
        clearView.layout(measuredWidth - clearView.measuredWidth, 0, measuredWidth, measuredHeight)

        urlView.layout(0, 0, 0 + urlView.measuredWidth, bottom)

        editActions
            .asSequence()
            .mapNotNull { it.view }
            .fold(urlView.measuredWidth) { usedWidth, view ->
                val viewLeft = usedWidth
                val viewRight = viewLeft + view.measuredWidth

                view.layout(viewLeft, 0, viewRight, measuredHeight)

                usedWidth + view.measuredWidth
            }
    }
}
