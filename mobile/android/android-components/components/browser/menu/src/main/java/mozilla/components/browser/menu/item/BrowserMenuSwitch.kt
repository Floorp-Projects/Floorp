/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.content.Context
import mozilla.components.browser.menu.R
import mozilla.components.concept.menu.candidate.CompoundMenuCandidate

/**
 * A simple browser menu switch.
 *
 * @param label The visible label of this menu item.
 * @param isCollapsingMenuLimit Whether this menu item can serve as the limit of a collapsing menu.
 * @param isSticky whether this item menu should not be scrolled offscreen (downwards or upwards
 * depending on the menu position).
 * @param initialState The initial value the checkbox should have.
 * @param listener Callback to be invoked when this menu item is checked.
 */
class BrowserMenuSwitch(
    label: String,
    override val isCollapsingMenuLimit: Boolean = false,
    override val isSticky: Boolean = false,
    initialState: () -> Boolean = { false },
    listener: (Boolean) -> Unit,
) : BrowserMenuCompoundButton(label, isCollapsingMenuLimit, isSticky, initialState, listener) {
    override fun getLayoutResource(): Int = R.layout.mozac_browser_menu_item_switch

    override fun asCandidate(context: Context) = super.asCandidate(context).copy(
        end = CompoundMenuCandidate.ButtonType.SWITCH,
    )
}
