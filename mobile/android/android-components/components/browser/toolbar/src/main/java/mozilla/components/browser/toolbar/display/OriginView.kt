/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.display

import android.animation.LayoutTransition
import android.content.Context
import android.graphics.Typeface
import android.util.AttributeSet
import android.util.TypedValue
import android.view.Gravity
import android.view.View
import android.widget.LinearLayout
import android.widget.TextView
import androidx.annotation.VisibleForTesting
import androidx.core.view.isVisible
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.browser.toolbar.R

private const val TITLE_VIEW_WEIGHT = 5.7f
private const val URL_VIEW_WEIGHT = 4.3f

/**
 * View displaying the URL and optionally the title of a website.
 */
internal class OriginView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
) : LinearLayout(context, attrs, defStyleAttr) {
    internal lateinit var toolbar: BrowserToolbar

    private val textSizeUrlNormal = context.resources.getDimension(
        R.dimen.mozac_browser_toolbar_url_textsize,
    )
    private val textSizeUrlWithTitle = context.resources.getDimension(
        R.dimen.mozac_browser_toolbar_url_with_title_textsize,
    )
    private val textSizeTitle = context.resources.getDimension(
        R.dimen.mozac_browser_toolbar_title_textsize,
    )

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal val urlView = TextView(context).apply {
        id = R.id.mozac_browser_toolbar_url_view
        setTextSize(TypedValue.COMPLEX_UNIT_PX, textSizeUrlNormal)
        gravity = Gravity.CENTER_VERTICAL

        setSingleLine()
        isClickable = true
        isFocusable = true

        setOnClickListener {
            if (onUrlClicked()) {
                toolbar.editMode()
            }
        }

        val fadingEdgeSize = resources.getDimensionPixelSize(
            R.dimen.mozac_browser_toolbar_url_fading_edge_size,
        )

        setFadingEdgeLength(fadingEdgeSize)
        isHorizontalFadingEdgeEnabled = fadingEdgeSize > 0
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal val titleView = TextView(context).apply {
        id = R.id.mozac_browser_toolbar_title_view
        visibility = View.GONE

        setTextSize(
            TypedValue.COMPLEX_UNIT_PX,
            textSizeTitle,
        )
        gravity = Gravity.CENTER_VERTICAL

        setSingleLine()

        val fadingEdgeSize = resources.getDimensionPixelSize(
            R.dimen.mozac_browser_toolbar_url_fading_edge_size,
        )

        setFadingEdgeLength(fadingEdgeSize)
        isHorizontalFadingEdgeEnabled = fadingEdgeSize > 0
    }

    init {
        orientation = VERTICAL

        addView(
            titleView,
            LayoutParams(
                LayoutParams.MATCH_PARENT,
                0,
                TITLE_VIEW_WEIGHT,
            ),
        )

        addView(
            urlView,
            LayoutParams(
                LayoutParams.MATCH_PARENT,
                0,
                URL_VIEW_WEIGHT,
            ),
        )

        layoutTransition = LayoutTransition()
    }

    internal var title: String
        get() = titleView.text.toString()
        set(value) {
            titleView.text = value

            titleView.isVisible = value.isNotEmpty()

            urlView.setTextSize(
                TypedValue.COMPLEX_UNIT_PX,
                if (value.isNotEmpty()) {
                    textSizeUrlWithTitle
                } else {
                    textSizeUrlNormal
                },
            )
        }

    internal var onUrlClicked: () -> Boolean = { true }

    fun setOnUrlLongClickListener(handler: ((View) -> Boolean)?) {
        urlView.isLongClickable = true
        titleView.isLongClickable = true

        urlView.setOnLongClickListener(handler)
        titleView.setOnLongClickListener(handler)
    }

    internal var url: CharSequence
        get() = urlView.text
        set(value) { urlView.text = value }

    /**
     * Sets the colour of the text to be displayed when the URL of the toolbar is empty.
     */
    var hintColor: Int
        get() = urlView.currentHintTextColor
        set(value) {
            urlView.setHintTextColor(value)
        }

    /**
     * Sets the text to be displayed when the URL of the toolbar is empty.
     */
    var hint: String
        get() = urlView.hint.toString()
        set(value) { urlView.hint = value }

    /**
     * Sets the colour of the text for title displayed in the toolbar.
     */
    var titleColor: Int
        get() = urlView.currentTextColor
        set(value) { titleView.setTextColor(value) }

    /**
     * Sets the colour of the text for the URL/search term displayed in the toolbar.
     */
    var textColor: Int
        get() = urlView.currentTextColor
        set(value) { urlView.setTextColor(value) }

    /**
     * Sets the size of the text for the title displayed in the toolbar.
     */
    var titleTextSize: Float
        get() = titleView.textSize
        set(value) { titleView.textSize = value }

    /**
     * Sets the size of the text for the URL/search term displayed in the toolbar.
     */
    var textSize: Float
        get() = urlView.textSize
        set(value) {
            urlView.textSize = value
        }

    /**
     * Sets the typeface of the text for the URL/search term displayed in the toolbar.
     */
    var typeface: Typeface
        get() = urlView.typeface
        set(value) {
            urlView.typeface = value
        }
}
