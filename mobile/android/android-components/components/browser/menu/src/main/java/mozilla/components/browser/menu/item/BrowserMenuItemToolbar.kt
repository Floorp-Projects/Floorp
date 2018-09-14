/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.support.v4.content.ContextCompat
import android.support.v7.widget.AppCompatImageButton
import android.util.TypedValue
import android.view.View
import android.widget.LinearLayout
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuItem
import mozilla.components.browser.menu.R
import mozilla.components.support.ktx.android.content.res.pxToDp

/**
 * A toolbar of buttons to show inside the browser menu.
 */
class BrowserMenuItemToolbar(
    private val items: List<Button>
) : BrowserMenuItem {
    override val visible: () -> Boolean = { true }

    override fun getLayoutResource() = R.layout.mozac_browser_menu_item_toolbar

    override fun bind(menu: BrowserMenu, view: View) {
        val layout = view as LinearLayout
        layout.removeAllViews()

        for (item in items) {
            val button = AppCompatImageButton(view.context)
            button.setImageResource(item.imageResource)

            val outValue = TypedValue()
            view.context.theme.resolveAttribute(
                    android.R.attr.selectableItemBackgroundBorderless,
                    outValue,
                    true)

            button.setBackgroundResource(outValue.resourceId)
            button.contentDescription = item.contentDescription
            button.setOnClickListener {
                item.listener.invoke()
                menu.dismiss()
            }

            if (item.iconTintColorResource != 0) {
                button.imageTintList = ContextCompat.getColorStateList(view.context, item.iconTintColorResource)
            }

            layout.addView(button,
                LinearLayout.LayoutParams(0, view.resources.pxToDp(ICON_HEIGHT_DP), 1f))
        }
    }

    /**
     * A button to be shown in a toolbar inside the browser menu.
     *
     * @param imageResource ID of a drawable resource to be shown as icon.
     * @param contentDescription The button's content description, used for accessibility support.
     * @param iconTintColorResource Optional ID of color resource to tint the icon.
     * @param listener Callback to be invoked when the button is pressed.
     */
    class Button(
        val imageResource: Int,
        val contentDescription: String,
        val iconTintColorResource: Int = 0,
        val listener: () -> Unit
    )

    companion object {
        private const val ICON_HEIGHT_DP = 24
    }
}
