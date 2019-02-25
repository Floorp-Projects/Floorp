/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.support.annotation.ColorRes
import android.support.v4.content.ContextCompat
import android.view.View
import android.widget.TextView
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuItem
import mozilla.components.browser.menu.R

private const val NO_ID = -1

/**
 * A simple browser menu item displaying text.
 *
 * @param label The visible label of this menu item.
 * @param listener Callback to be invoked when this menu item is clicked.
 */
class SimpleBrowserMenuItem(
    private val label: String,
    @ColorRes
    private val textColorResource: Int = NO_ID,
    private val listener: () -> Unit
) : BrowserMenuItem {
    override var visible: () -> Boolean = { true }

    override fun getLayoutResource() = R.layout.mozac_browser_menu_item_simple

    override fun bind(menu: BrowserMenu, view: View) {
        (view as TextView).text = label
        if (textColorResource != NO_ID) {
            view.setTextColor(ContextCompat.getColor(view.context, textColorResource))
        }
        view.setOnClickListener {
            listener.invoke()
            menu.dismiss()
        }
    }
}
