/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.content.Context
import android.view.View
import android.widget.TextView
import androidx.annotation.ColorInt
import androidx.annotation.ColorRes
import androidx.core.content.ContextCompat
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuItem
import mozilla.components.browser.menu.R
import mozilla.components.browser.menu.ext.addRippleEffect
import mozilla.components.concept.menu.candidate.ContainerStyle
import mozilla.components.concept.menu.candidate.MenuCandidate
import mozilla.components.concept.menu.candidate.TextMenuCandidate
import mozilla.components.concept.menu.candidate.TextStyle

/**
 * A menu item for displaying text with a highlight state which sets the
 * background of the menu item.
 *
 * @param label The default visible label of this menu item.
 * @param textColorResource Optional ID of color resource to tint the text.
 * @param textSize The size of the label.
 * @param backgroundTint Tint for the menu item background color
 * @param isCollapsingMenuLimit Whether this menu item can serve as the limit of a collapsing menu.
 * @param isSticky whether this item menu should not be scrolled offscreen (downwards or upwards
 * depending on the menu position).
 * @param isHighlighted Whether or not to display the highlight
 * @param listener Callback to be invoked when this menu item is clicked.
 */
@Suppress("LongParameterList")
class SimpleBrowserMenuHighlightableItem(
    private val label: String,
    @ColorRes private val textColorResource: Int = NO_ID,
    private val textSize: Float = NO_ID.toFloat(),
    @ColorInt val backgroundTint: Int,
    override val isCollapsingMenuLimit: Boolean = false,
    override val isSticky: Boolean = false,
    var isHighlighted: () -> Boolean = { true },
    private val listener: () -> Unit = {},
) : BrowserMenuItem {

    override var visible: () -> Boolean = { true }
    private var wasHighlighted = false

    override fun getLayoutResource() = R.layout.mozac_browser_menu_item_simple

    override fun bind(menu: BrowserMenu, view: View) {
        bindText(view)

        view.setOnClickListener {
            listener.invoke()
            menu.dismiss()
        }

        wasHighlighted = isHighlighted()
        updateHighlight(view, wasHighlighted)
    }

    private fun bindText(view: View) {
        val textView = view as TextView
        textView.text = label
        textView.addRippleEffect()

        if (textColorResource != NO_ID) {
            textView.setColorResource(textColorResource)
        }

        if (textSize != NO_ID.toFloat()) {
            textView.textSize = textSize
        }
    }

    override fun invalidate(view: View) {
        val isNowHighlighted = isHighlighted()
        if (isNowHighlighted != wasHighlighted) {
            wasHighlighted = isNowHighlighted
            updateHighlight(view, isNowHighlighted)
        }
    }

    private fun updateHighlight(view: View, isHighlighted: Boolean) {
        val textView = view as TextView

        if (isHighlighted) {
            textView.setBackgroundColor(backgroundTint)
        } else {
            textView.addRippleEffect()
        }
    }

    override fun asCandidate(context: Context): MenuCandidate {
        val textStyle = TextStyle(
            size = if (textSize == NO_ID.toFloat()) null else textSize,
            color = if (textColorResource == NO_ID) null else ContextCompat.getColor(context, textColorResource),
        )
        val containerStyle = ContainerStyle(isVisible = visible())
        return TextMenuCandidate(
            label,
            textStyle = textStyle,
            containerStyle = containerStyle,
            onClick = listener,
        )
    }
}
