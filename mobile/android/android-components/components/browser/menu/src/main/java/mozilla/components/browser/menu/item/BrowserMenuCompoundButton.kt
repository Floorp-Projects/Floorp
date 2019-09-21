/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.view.View
import android.widget.CompoundButton
import androidx.annotation.VisibleForTesting
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuItem
import java.lang.reflect.Modifier.PRIVATE

/**
 * A browser menu compound button. A basic sub-class would only have to provide a layout resource to
 * satisfy [BrowserMenuItem.getLayoutResource] which contains a [View] that inherits from [CompoundButton].
 *
 * @param label The visible label of this menu item.
 * @param initialState The initial value the checkbox should have.
 * @param listener Callback to be invoked when this menu item is checked.
 */
abstract class BrowserMenuCompoundButton(
    @VisibleForTesting(otherwise = PRIVATE)
    val label: String,
    private val initialState: () -> Boolean = { false },
    private val listener: (Boolean) -> Unit
) : BrowserMenuItem {
    override var visible: () -> Boolean = { true }

    override fun bind(menu: BrowserMenu, view: View) {
        (view as CompoundButton).apply {
            text = label
            isChecked = initialState()
            setOnCheckedChangeListener { _, checked ->
                listener(checked)
                menu.dismiss()
            }
        }
    }
}
