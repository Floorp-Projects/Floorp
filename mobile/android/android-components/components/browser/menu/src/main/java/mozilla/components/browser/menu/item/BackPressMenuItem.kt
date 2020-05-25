/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.view.View
import androidx.annotation.ColorRes
import androidx.annotation.DrawableRes
import mozilla.components.browser.menu.BrowserMenu

/**
 * A back press menu item for a nested sub menu entry.
 *
 * @param backPressListener Callback to be invoked when the back press menu item is clicked.
 */
class BackPressMenuItem(
    label: String,
    @DrawableRes
    imageResource: Int,
    @ColorRes
    iconTintColorResource: Int = NO_ID,
    @ColorRes
    textColorResource: Int = NO_ID,
    private var backPressListener: () -> Unit = {}
) : BrowserMenuImageText(label, imageResource, iconTintColorResource, textColorResource) {

    /**
     * Binds the view according to its super, but use [backPressListener] for on view clicks.
     */
    override fun bind(menu: BrowserMenu, view: View) {
        super.bind(menu, view)

        view.setOnClickListener {
            backPressListener.invoke()
            menu.dismiss()
        }
    }
    /**
     * Sets and replaces the existing [backPressListener] for the back press item.
     */
    fun setListener(onClickListener: () -> Unit) {
        backPressListener = onClickListener
    }
}
