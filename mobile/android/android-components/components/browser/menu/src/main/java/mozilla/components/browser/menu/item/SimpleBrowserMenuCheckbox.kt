/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.view.View
import android.widget.CheckBox
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuItem
import mozilla.components.browser.menu.R

/**
 * A simple browser menu checkbox.
 *
 * @param label The visible label of this menu item.
 * @param initialState The initial value the checkbox should have.
 * @param listener Callback to be invoked when this menu item is checked.
 */
class SimpleBrowserMenuCheckbox(
    private val label: String,
    private var initialState: Boolean = false,
    private val listener: (Boolean) -> Unit
) : BrowserMenuItem {
    override var visible: () -> Boolean = { true }

    override fun getLayoutResource() = R.layout.mozac_browser_menu_checkbox

    override fun bind(menu: BrowserMenu, view: View) {
        (view as CheckBox).text = label
        view.setOnCheckedChangeListener { _, checked ->
            listener.invoke(checked)
            menu.dismiss()
        }
    }
}
