/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.content.Context
import android.view.View
import android.widget.TextView
import androidx.annotation.ColorRes
import androidx.annotation.DrawableRes
import androidx.appcompat.widget.AppCompatImageView
import androidx.core.content.ContextCompat
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.R
import mozilla.components.browser.menu.ext.asCandidateList
import mozilla.components.concept.menu.candidate.ContainerStyle
import mozilla.components.concept.menu.candidate.DrawableMenuIcon
import mozilla.components.concept.menu.candidate.NestedMenuCandidate
import mozilla.components.concept.menu.candidate.TextStyle

/**
 * A parent menu item for displaying text and an image icon with a nested sub menu.
 * It handles back pressing if the sub menu contains a [BackPressMenuItem].
 *
 * @param label The visible label of this menu item.
 * @param imageResource ID of a drawable resource to be shown as icon.
 * @param iconTintColorResource Optional ID of color resource to tint the icon.
 * @param textColorResource Optional ID of color resource to tint the text.
 * @property subMenu Target sub menu to be shown when this menu item is clicked.
 * @param isCollapsingMenuLimit Whether this menu item can serve as the limit of a collapsing menu.
 * @param isSticky whether this item menu should not be scrolled offscreen (downwards or upwards
 * depending on the menu position).
 * @param endOfMenuAlwaysVisible when is set to true makes sure the bottom of the menu is always visible
 * otherwise, the top of the menu is always visible.
 */
@Suppress("LongParameterList")
class ParentBrowserMenuItem(
    internal val label: String,
    @DrawableRes
    private val imageResource: Int,
    @ColorRes
    private val iconTintColorResource: Int = NO_ID,
    @ColorRes
    private val textColorResource: Int = NO_ID,
    internal val subMenu: BrowserMenu,
    override val isCollapsingMenuLimit: Boolean = false,
    override val isSticky: Boolean = false,
    endOfMenuAlwaysVisible: Boolean = false,
) : AbstractParentBrowserMenuItem(subMenu, isCollapsingMenuLimit, endOfMenuAlwaysVisible) {

    override var visible: () -> Boolean = { true }
    override fun getLayoutResource() = R.layout.mozac_browser_menu_item_image_text

    override fun bind(menu: BrowserMenu, view: View) {
        bindText(view)
        bindImage(view)
        bindBackPress(menu, view)

        super.bind(menu, view)
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
        val overflowView = view.findViewById<AppCompatImageView>(R.id.overflowImage)
        with(overflowView) {
            visibility = View.VISIBLE
            setTintResource(iconTintColorResource)
        }
    }

    private fun bindBackPress(menu: BrowserMenu, view: View) {
        val backPressMenuItem =
            subMenu.adapter.visibleItems.find { it is BackPressMenuItem } as? BackPressMenuItem
        backPressMenuItem?.let {
            backPressMenuItem.setListener {
                onBackPressed(menu, view)
            }
        }
    }

    override fun asCandidate(context: Context) = NestedMenuCandidate(
        id = hashCode(),
        text = label,
        start = DrawableMenuIcon(
            context,
            resource = imageResource,
            tint = if (iconTintColorResource == NO_ID) null else ContextCompat.getColor(context, iconTintColorResource),
        ),
        subMenuItems = subMenu.adapter.visibleItems.asCandidateList(context),
        textStyle = TextStyle(
            color = if (textColorResource == NO_ID) null else ContextCompat.getColor(context, textColorResource),
        ),
        containerStyle = ContainerStyle(isVisible = visible()),
    )
}
