/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.content.Context
import android.view.View
import androidx.annotation.DrawableRes
import androidx.annotation.VisibleForTesting
import androidx.appcompat.widget.SwitchCompat
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.R
import mozilla.components.concept.menu.candidate.CompoundMenuCandidate
import mozilla.components.concept.menu.candidate.DrawableMenuIcon
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
        super.bind(menu, view)
        bindImage(view as SwitchCompat)
    }

    private fun bindImage(switch: SwitchCompat) {
        switch.setCompoundDrawablesWithIntrinsicBounds(
            imageResource, 0, 0, 0)
    }

    override fun asCandidate(context: Context) = super.asCandidate(context).copy(
        start = DrawableMenuIcon(context, imageResource),
        end = CompoundMenuCandidate.ButtonType.SWITCH
    )
}
