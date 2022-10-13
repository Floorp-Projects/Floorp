/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.content.Context
import android.view.View
import android.widget.ImageView
import android.widget.TextView
import androidx.annotation.ColorRes
import androidx.annotation.DrawableRes
import androidx.appcompat.widget.AppCompatImageView
import androidx.core.content.ContextCompat
import androidx.core.content.ContextCompat.getColor
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuItem
import mozilla.components.browser.menu.R
import mozilla.components.concept.menu.candidate.ContainerStyle
import mozilla.components.concept.menu.candidate.DrawableMenuIcon
import mozilla.components.concept.menu.candidate.MenuCandidate
import mozilla.components.concept.menu.candidate.TextMenuCandidate
import mozilla.components.concept.menu.candidate.TextStyle

internal const val NO_ID = -1

internal fun ImageView.setTintResource(@ColorRes tintColorResource: Int) {
    if (tintColorResource != NO_ID) {
        imageTintList = ContextCompat.getColorStateList(context, tintColorResource)
    }
}

internal fun TextView.setColorResource(@ColorRes textColorResource: Int) {
    if (textColorResource != NO_ID) {
        setTextColor(ContextCompat.getColor(context, textColorResource))
    }
}

/**
 * A menu item for displaying text with an image icon.
 *
 * @param label The visible label of this menu item.
 * @param imageResource ID of a drawable resource to be shown as icon.
 * @param iconTintColorResource Optional ID of color resource to tint the icon.
 * @param textColorResource Optional ID of color resource to tint the text.
 * @param isCollapsingMenuLimit Whether this menu item can serve as the limit of a collapsing menu.
 * @param isSticky whether this item menu should not be scrolled offscreen (downwards or upwards
 * depending on the menu position).
 * @param listener Callback to be invoked when this menu item is clicked.
 */
@Suppress("LongParameterList")
open class BrowserMenuImageText(
    private val label: String,
    @DrawableRes
    internal val imageResource: Int,
    @ColorRes
    internal val iconTintColorResource: Int = NO_ID,
    @ColorRes
    private val textColorResource: Int = NO_ID,
    override val isCollapsingMenuLimit: Boolean = false,
    override val isSticky: Boolean = false,
    private val listener: () -> Unit = {},
) : BrowserMenuItem {

    override var visible: () -> Boolean = { true }

    override fun getLayoutResource() = R.layout.mozac_browser_menu_item_image_text

    override fun bind(menu: BrowserMenu, view: View) {
        bindText(view)

        bindImage(view)

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

    private fun bindImage(view: View) {
        val imageView = view.findViewById<AppCompatImageView>(R.id.image)
        with(imageView) {
            setImageResource(imageResource)
            setTintResource(iconTintColorResource)
        }
    }

    override fun asCandidate(context: Context): MenuCandidate = TextMenuCandidate(
        label,
        start = DrawableMenuIcon(
            context,
            resource = imageResource,
            tint = if (iconTintColorResource == NO_ID) null else getColor(context, iconTintColorResource),
        ),
        textStyle = TextStyle(
            color = if (textColorResource == NO_ID) null else getColor(context, textColorResource),
        ),
        containerStyle = ContainerStyle(isVisible = visible()),
        onClick = listener,
    )
}
