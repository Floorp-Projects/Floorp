/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import mozilla.components.browser.menu.R

/**
 * A simple browser menu switch.
 *
 * @param label The visible label of this menu item.
 * @param initialState The initial value the checkbox should have.
 * @param listener Callback to be invoked when this menu item is checked.
 */
class BrowserMenuSwitch(
    label: String,
    initialState: () -> Boolean = { false },
    listener: (Boolean) -> Unit
) : BrowserMenuCompoundButton(label, initialState, listener) {
    override fun getLayoutResource(): Int = R.layout.mozac_browser_menu_item_switch
}
