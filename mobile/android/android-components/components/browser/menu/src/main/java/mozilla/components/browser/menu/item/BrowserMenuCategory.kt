/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.content.Context
import android.graphics.Typeface
import android.view.View
import android.widget.TextView
import androidx.annotation.ColorRes
import androidx.core.content.ContextCompat.getColor
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuItem
import mozilla.components.browser.menu.R
import mozilla.components.concept.menu.candidate.ContainerStyle
import mozilla.components.concept.menu.candidate.DecorativeTextMenuCandidate
import mozilla.components.concept.menu.candidate.TextAlignment
import mozilla.components.concept.menu.candidate.TextStyle
import mozilla.components.concept.menu.candidate.TypefaceStyle

/**
 * A browser menu item displaying styleable text, usable for menu categories
 *
 * @param label The visible label of this menu item.
 * @param textSize: The size of the label.
 * @param textColorResource: The color resource to apply to the text.
 * @param backgroundColorResource: The color resource to apply to the item background.
 * @param textStyle: The style to apply to the text.
 * @param textAlignment The alignment of text
 * @param isCollapsingMenuLimit Whether this menu item can serve as the limit of a collapsing menu.
 * @param isSticky whether this item menu should not be scrolled offscreen (downwards or upwards
 * depending on the menu position).
 */
@Suppress("LongParameterList")
class BrowserMenuCategory(
    internal val label: String,
    private val textSize: Float = NO_ID.toFloat(),
    @ColorRes
    private val textColorResource: Int = NO_ID,
    @ColorRes
    private val backgroundColorResource: Int = NO_ID,
    @TypefaceStyle private val textStyle: Int = Typeface.BOLD,
    @TextAlignment private val textAlignment: Int = View.TEXT_ALIGNMENT_VIEW_START,
    override val isCollapsingMenuLimit: Boolean = false,
    override val isSticky: Boolean = false,
) : BrowserMenuItem {
    override var visible: () -> Boolean = { true }

    override fun getLayoutResource() = R.layout.mozac_browser_menu_category

    override fun bind(menu: BrowserMenu, view: View) {
        val textView = view as TextView
        textView.text = label

        if (textSize != NO_ID.toFloat()) {
            textView.textSize = textSize
        }

        if (textColorResource != NO_ID) {
            textView.setColorResource(textColorResource)
        }

        textView.setTypeface(textView.typeface, textStyle)
        textView.textAlignment = textAlignment

        if (backgroundColorResource != NO_ID) {
            view.setBackgroundResource(backgroundColorResource)
        }
    }

    override fun asCandidate(context: Context) = DecorativeTextMenuCandidate(
        label,
        textStyle = TextStyle(
            size = if (textSize == NO_ID.toFloat()) null else textSize,
            color = if (textColorResource == NO_ID) null else getColor(context, textColorResource),
            textStyle = textStyle,
            textAlignment = textAlignment,
        ),
        containerStyle = ContainerStyle(isVisible = visible()),
    )
}
