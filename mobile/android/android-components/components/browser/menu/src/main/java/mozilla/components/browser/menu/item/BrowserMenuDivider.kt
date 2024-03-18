/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.content.Context
import android.view.View
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuItem
import mozilla.components.browser.menu.R
import mozilla.components.concept.menu.candidate.ContainerStyle
import mozilla.components.concept.menu.candidate.DividerMenuCandidate

/**
 * A browser menu item to display a horizontal divider.
 */
class BrowserMenuDivider : BrowserMenuItem {
    override var visible: () -> Boolean = { true }

    override val interactiveCount: () -> Int = { 0 }

    override fun getLayoutResource() = R.layout.mozac_browser_menu_item_divider

    override fun bind(menu: BrowserMenu, view: View) = Unit

    override fun asCandidate(context: Context) = DividerMenuCandidate(
        containerStyle = ContainerStyle(isVisible = visible()),
    )
}
