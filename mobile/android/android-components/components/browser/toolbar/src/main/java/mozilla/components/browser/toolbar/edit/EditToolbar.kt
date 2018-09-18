/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.edit

import android.annotation.SuppressLint
import android.content.Context
import android.text.InputType
import android.view.Gravity
import android.view.ViewGroup
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputMethodManager
import android.widget.ImageView
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.browser.toolbar.R
import mozilla.components.support.ktx.android.content.res.pxToDp
import mozilla.components.support.ktx.android.view.showKeyboard
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
class EditToolbar(
    context: Context,
    private val toolbar: BrowserToolbar
) : ViewGroup(context) {
    internal val urlView = InlineAutocompleteEditText(context).apply {
        imeOptions = EditorInfo.IME_ACTION_GO or EditorInfo.IME_FLAG_NO_EXTRACT_UI or EditorInfo.IME_FLAG_NO_FULLSCREEN
        gravity = Gravity.CENTER_VERTICAL
        background = null
        setLines(1)
        textSize = URL_TEXT_SIZE
        inputType = InputType.TYPE_TEXT_VARIATION_URI or InputType.TYPE_CLASS_TEXT

        val padding = resources.pxToDp(URL_PADDING_DP)
        setPadding(padding, padding, padding, padding)
        setSelectAllOnFocus(true)

        setOnCommitListener { toolbar.onUrlEntered(text.toString()) }
    }

    private val cancelView = ImageView(context).apply {
        val padding = resources.pxToDp(CANCEL_PADDING_DP)
        setPadding(padding, padding, padding, padding)
        setImageResource(mozilla.components.ui.icons.R.drawable.mozac_ic_close)
        contentDescription = context.getString(R.string.mozac_close_button_description)

        setOnClickListener {
            toolbar.displayMode()
        }
    }

    init {
        addView(urlView)
        addView(cancelView)
    }

    /**
     * Updates the URL. This should only be called if the toolbar is not in editing mode. Otherwise
     * this might override the URL the user is currently typing.
     */
    fun updateUrl(url: String) {
        urlView.setText(url)
    }

    /**
     * Focus the URL editing component and show the virtual keyboard if needed.
     */
    fun focus() {
        urlView.showKeyboard(flags = InputMethodManager.SHOW_FORCED)
    }

    // We measure the views manually to avoid overhead by using complex ViewGroup implementations
    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        // This toolbar is using the full size provided by the parent
        val width = MeasureSpec.getSize(widthMeasureSpec)
        val height = MeasureSpec.getSize(heightMeasureSpec)

        setMeasuredDimension(width, height)

        // The icon fills the whole height and has a square shape
        val iconSquareSpec = MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY)
        cancelView.measure(iconSquareSpec, iconSquareSpec)

        val urlWidthSpec = MeasureSpec.makeMeasureSpec(width - height, MeasureSpec.EXACTLY)
        urlView.measure(urlWidthSpec, heightMeasureSpec)
    }

    // We layout the toolbar ourselves to avoid the overhead from using complex ViewGroup implementations
    override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
        cancelView.layout(measuredWidth - cancelView.measuredWidth, 0, measuredWidth, measuredHeight)

        urlView.layout(0, 0, measuredWidth - cancelView.measuredWidth, bottom)
    }

    companion object {
        private const val URL_TEXT_SIZE = 15f
        private const val URL_PADDING_DP = 8
        private const val CANCEL_PADDING_DP = 16
    }
}
