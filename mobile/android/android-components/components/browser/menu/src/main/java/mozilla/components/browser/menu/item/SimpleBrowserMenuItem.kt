/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.content.Context
import android.view.View
import android.widget.TextView
import androidx.annotation.ColorRes
import androidx.core.content.ContextCompat.getColor
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuItem
import mozilla.components.browser.menu.R
import mozilla.components.concept.menu.candidate.ContainerStyle
import mozilla.components.concept.menu.candidate.DecorativeTextMenuCandidate
import mozilla.components.concept.menu.candidate.MenuCandidate
import mozilla.components.concept.menu.candidate.TextMenuCandidate
import mozilla.components.concept.menu.candidate.TextStyle

/**
 * A simple browser menu item displaying text.
 *
 * @param label The visible label of this menu item.
 * @param textSize: The size of the label.
 * @param textColorResource: The color resource to apply to the text.
 * @param isCollapsingMenuLimit Whether this menu item can serve as the limit of a collapsing menu.
 * @param listener Callback to be invoked when this menu item is clicked.
 */
class SimpleBrowserMenuItem(
    private val label: String,
    private val textSize: Float = NO_ID.toFloat(),
    @ColorRes
    private val textColorResource: Int = NO_ID,
    override val isCollapsingMenuLimit: Boolean = false,
    private val listener: (() -> Unit)? = null,
) : BrowserMenuItem {
    override var visible: () -> Boolean = { true }

    override fun getLayoutResource() = R.layout.mozac_browser_menu_item_simple

    override fun bind(menu: BrowserMenu, view: View) {
        val textView = view as TextView
        textView.text = label

        if (textSize != NO_ID.toFloat()) {
            textView.textSize = textSize
        }

        textView.setColorResource(textColorResource)

        if (listener != null) {
            textView.setOnClickListener {
                listener.invoke()
                menu.dismiss()
            }
        } else {
            // Remove the ripple effect
            textView.background = null
        }
    }

    override fun asCandidate(context: Context): MenuCandidate {
        val textStyle = TextStyle(
            size = if (textSize == NO_ID.toFloat()) null else textSize,
            color = if (textColorResource == NO_ID) null else getColor(context, textColorResource),
        )
        val containerStyle = ContainerStyle(isVisible = visible())
        return if (listener != null) {
            TextMenuCandidate(
                label,
                textStyle = textStyle,
                containerStyle = containerStyle,
                onClick = listener,
            )
        } else {
            DecorativeTextMenuCandidate(
                label,
                textStyle = textStyle,
                containerStyle = containerStyle,
            )
        }
    }
}
