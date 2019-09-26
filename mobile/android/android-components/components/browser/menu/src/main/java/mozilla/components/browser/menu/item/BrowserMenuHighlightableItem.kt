/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.view.View
import androidx.annotation.ColorRes
import androidx.annotation.DrawableRes
import androidx.appcompat.widget.AppCompatImageView
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.R
import mozilla.components.support.ktx.android.content.res.resolveAttribute

private val defaultHighlight = BrowserMenuHighlightableItem.Highlight(0, 0, 0, 0)

/**
 * A menu item for displaying text with an image icon and a highlight state which sets the
 * background of the menu item and a second image icon to the right of the text.
 *
 * @param label The visible label of this menu item.
 * @param startImageResource ID of a drawable resource to be shown as a leftmost icon.
 * @param iconTintColorResource Optional ID of color resource to tint the icon.
 * @param textColorResource Optional ID of color resource to tint the text.
 * @param highlight Highlight object storing the background drawable and additional icon
 * @param isHighlighted Whether or not to display the highlight
 * @param listener Callback to be invoked when this menu item is clicked.
 */
class BrowserMenuHighlightableItem(
    private val label: String,
    @DrawableRes
    private val startImageResource: Int,
    @DrawableRes
    @ColorRes
    private val iconTintColorResource: Int = NO_ID,
    @ColorRes
    private val textColorResource: Int = NO_ID,
    val highlight: Highlight,
    val isHighlighted: () -> Boolean = { true },
    private val listener: () -> Unit = {}
) : BrowserMenuImageText(
    label,
    startImageResource,
    iconTintColorResource,
    textColorResource,
    listener
) {

    /**
     * @deprecated Use the new constructor
     */
    constructor(
        label: String,
        @DrawableRes
        imageResource: Int,
        @DrawableRes
        @ColorRes
        iconTintColorResource: Int = NO_ID,
        @ColorRes
        textColorResource: Int = NO_ID,
        highlight: Highlight? = null,
        listener: () -> Unit = {}
    ) : this(
        label,
        imageResource,
        iconTintColorResource,
        textColorResource,
        highlight ?: defaultHighlight,
        { highlight != null },
        listener
    )

    private var wasHighlighted = false

    override fun getLayoutResource() = R.layout.mozac_browser_menu_highlightable_item

    override fun bind(menu: BrowserMenu, view: View) {
        super.bind(menu, view)
        wasHighlighted = isHighlighted()
        updateHighlight(view, wasHighlighted)

        val highlightImageView = view.findViewById<AppCompatImageView>(R.id.end_image)
        highlightImageView.setTintResource(iconTintColorResource)
    }

    override fun invalidate(view: View) {
        val isNowHighlighted = isHighlighted()
        if (isNowHighlighted != wasHighlighted) {
            wasHighlighted = isNowHighlighted
            updateHighlight(view, isNowHighlighted)
        }
    }

    private fun updateHighlight(view: View, isHighlighted: Boolean) {
        val startImageView = view.findViewById<AppCompatImageView>(R.id.image)
        val endImageView = view.findViewById<AppCompatImageView>(R.id.end_image)

        if (isHighlighted) {
            view.setBackgroundResource(highlight.backgroundResource)
            with(startImageView) {
                if (highlight.startImageResource != NO_ID) {
                    setImageResource(highlight.startImageResource)
                }
            }
            with(endImageView) {
                if (highlight.endImageResource != NO_ID) {
                    setImageResource(highlight.endImageResource)
                    visibility = View.VISIBLE
                }
            }
        } else {
            val selectableItemBackground =
                view.context.theme.resolveAttribute(android.R.attr.selectableItemBackground)

            view.setBackgroundResource(selectableItemBackground)
            with(endImageView) {
                setImageResource(0)
                visibility = View.GONE
            }
        }
    }

    class Highlight(
        @DrawableRes
        val startImageResource: Int = NO_ID,
        @DrawableRes
        val endImageResource: Int = NO_ID,
        @DrawableRes
        val backgroundResource: Int,
        @ColorRes
        val colorResource: Int
    )
}
