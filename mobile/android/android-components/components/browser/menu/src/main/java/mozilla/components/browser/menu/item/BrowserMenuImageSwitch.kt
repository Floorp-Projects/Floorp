/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.view.View
import android.widget.Switch
import androidx.annotation.DrawableRes
import androidx.annotation.VisibleForTesting
import androidx.appcompat.widget.AppCompatImageView
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.R
import java.lang.reflect.Modifier

/**
 * A simple browser menu switch.
 *
 * @param imageResource ID of a drawable resource to be shown as icon.
 * @param label The visible label of this menu item.
 * @param initialState The initial value the checkbox should have.
 * @param listener Callback to be invoked when this menu item is checked.
 */
class BrowserMenuImageSwitch(
    @VisibleForTesting(otherwise = Modifier.PRIVATE)
    @DrawableRes
    val imageResource: Int,
    label: String,
    initialState: () -> Boolean = { false },
    listener: (Boolean) -> Unit
) : BrowserMenuCompoundButton(label, initialState, listener) {
    override fun getLayoutResource(): Int = R.layout.mozac_browser_menu_item_image_switch

    override fun bind(menu: BrowserMenu, view: View) {
        super.bind(menu, view.findViewById<Switch>(R.id.switch_widget))
        bindImage(view)
    }

    private fun bindImage(view: View) {
        val imageView = view.findViewById<AppCompatImageView>(R.id.image)
        imageView.setImageResource(imageResource)
    }
}
