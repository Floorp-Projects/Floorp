/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.view.View
import android.widget.TextView
import androidx.annotation.ColorRes
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuItem
import mozilla.components.browser.menu.R

/**
 * A simple browser menu item displaying text.
 *
 * @param label The visible label of this menu item.
 * @param textSize: The size of the label.
 * @param textColorResource: The color resource to apply to the text.
 * @param listener Callback to be invoked when this menu item is clicked.
 */
class SimpleBrowserMenuItem(
    private val label: String,
    private val textSize: Float = NO_ID.toFloat(),
    @ColorRes
    private val textColorResource: Int = NO_ID,
    private val listener: (() -> Unit)? = null
) : BrowserMenuItem {
    override var visible: () -> Boolean = { true }

    override fun getLayoutResource() = R.layout.mozac_browser_menu_item_simple

    override fun bind(menu: BrowserMenu, view: View) {
        val textView = view as TextView
        textView.text = label

        if (textSize != NO_ID.toFloat()) {
            textView.textSize = textSize
        }

        textView.setColorResource(textColorResource)

        if (listener != null) {
            textView.setOnClickListener {
                listener.invoke()
                menu.dismiss()
            }
        } else {
            // Remove the ripple effect
            textView.background = null
        }
    }
}
