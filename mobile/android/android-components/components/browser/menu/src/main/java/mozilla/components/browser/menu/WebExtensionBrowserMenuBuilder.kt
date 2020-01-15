/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu

import android.content.Context
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.store.BrowserStore

/**
 * Browser menu builder with web extension support. It allows [WebExtensionBrowserMenu] to add
 * web extension browser actions.
 *
 * @param store [BrowserStore] required to render web extension browser actions
 * @param appendExtensionActionAtStart true if web extensions appear at the top (start) of the menu,
 * false if web extensions appear at the bottom of the menu. Default to false (bottom).
 */
class WebExtensionBrowserMenuBuilder(
    items: List<BrowserMenuItem>,
    extras: Map<String, Any> = emptyMap(),
    endOfMenuAlwaysVisible: Boolean = false,
    private val store: BrowserStore,
    private val appendExtensionActionAtStart: Boolean = false
) : BrowserMenuBuilder(items, extras, endOfMenuAlwaysVisible) {

    /**
     * Builds and returns a browser menu with combination of [items] and web extension browser actions.
     */
    override fun build(context: Context): BrowserMenu {
        val extensionMenuItems =
            WebExtensionBrowserMenu.getOrUpdateWebExtensionMenuItems(store.state, store.state.selectedTab)

        val menuItems =
            if (appendExtensionActionAtStart) {
                extensionMenuItems + items
            } else {
                items + extensionMenuItems
            }

        val adapter = BrowserMenuAdapter(context, menuItems)
        return WebExtensionBrowserMenu(adapter, store)
    }
}
