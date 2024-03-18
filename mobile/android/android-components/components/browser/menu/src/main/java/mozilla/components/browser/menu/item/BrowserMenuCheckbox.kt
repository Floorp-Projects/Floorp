/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.content.Context
import mozilla.components.browser.menu.R
import mozilla.components.concept.menu.candidate.CompoundMenuCandidate

/**
 * A simple browser menu checkbox.
 *
 * @param label The visible label of this menu item.
 * @param initialState The initial value the checkbox should have.
 * @param isCollapsingMenuLimit Whether this menu item can serve as the limit of a collapsing menu.
 * @param isSticky whether this item menu should not be scrolled offscreen (downwards or upwards
 * depending on the menu position).
 * @param listener Callback to be invoked when this menu item is checked.
 */
class BrowserMenuCheckbox(
    label: String,
    initialState: () -> Boolean = { false },
    override val isCollapsingMenuLimit: Boolean = false,
    override val isSticky: Boolean = false,
    listener: (Boolean) -> Unit,
) : BrowserMenuCompoundButton(label, isCollapsingMenuLimit, isSticky, initialState, listener) {
    override fun getLayoutResource() = R.layout.mozac_browser_menu_item_checkbox

    override fun asCandidate(context: Context) = super.asCandidate(context).copy(
        end = CompoundMenuCandidate.ButtonType.CHECKBOX,
    )
}
