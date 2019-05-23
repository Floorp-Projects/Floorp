/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.view.View
import android.widget.TextView
import androidx.annotation.ColorRes
import androidx.annotation.DrawableRes
import androidx.appcompat.widget.AppCompatImageView
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuItem
import mozilla.components.browser.menu.R

/**
 * A menu item for displaying text with an image icon and a highlight state which sets the
 * background of the menu item and a second image icon to the right of the text.
 *
 * @param label The visible label of this menu item.
 * @param imageResource ID of a drawable resource to be shown as a leftmost icon.
 * @param iconTintColorResource Optional ID of color resource to tint the icon.
 * @param textColorResource Optional ID of color resource to tint the text.
 * @param highlight Optional highlight object storing the background drawable and additional icon
 * @param listener Callback to be invoked when this menu item is clicked.
 */
class BrowserMenuHighlightableItem(
    private val label: String,
    @DrawableRes
    private val imageResource: Int,
    @DrawableRes
    @ColorRes
    private val iconTintColorResource: Int = NO_ID,
    @ColorRes
    private val textColorResource: Int = NO_ID,
    private val highlight: BrowserMenuHighlightableItem.Highlight? = null,
    private val listener: () -> Unit = {}
) : BrowserMenuItem {

    override var visible: () -> Boolean = { true }

    override fun getLayoutResource() = R.layout.mozac_browser_menu_highlightable_item

    override fun bind(menu: BrowserMenu, view: View) {
        bindText(view)
        bindImages(view)
        bindHighlight(view)

        view.setOnClickListener {
            listener.invoke()
            menu.dismiss()
        }
    }

    private fun bindText(view: View) {
        val textView = view.findViewById<TextView>(R.id.text)
        textView.text = label
        textView.setColorResource(textColorResource)
    }

    private fun bindImages(view: View) {
        val leftImageView = view.findViewById<AppCompatImageView>(R.id.image)
        with(leftImageView) {
            setImageResource(imageResource)
            setTintResource(iconTintColorResource)
        }
    }

    private fun bindHighlight(view: View) {
        val highlightImageView = view.findViewById<AppCompatImageView>(R.id.highlight_image)

        if (highlight == null) {
            view.background = null
            highlightImageView.setImageDrawable(null)
            highlightImageView.visibility = View.GONE
            return
        }

        view.setBackgroundResource(highlight.backgroundResource)
        with(highlightImageView) {
            visibility = View.VISIBLE
            setImageResource(highlight.imageResource)
            setTintResource(iconTintColorResource)
        }
    }

    class Highlight(
        val imageResource: Int = NO_ID,
        val backgroundResource: Int = NO_ID
    )
}
