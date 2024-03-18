/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu

import android.content.Context

/**
 * Helper class for building browser menus.
 *
 * @param items List of BrowserMenuItem objects to compose the menu from.
 * @param extras Map of extra values that are added to emitted facts
 * @param endOfMenuAlwaysVisible when is set to true makes sure the bottom of the menu is always visible otherwise,
 *  the top of the menu is always visible.
 */
open class BrowserMenuBuilder(
    val items: List<BrowserMenuItem>,
    val extras: Map<String, Any> = emptyMap(),
    val endOfMenuAlwaysVisible: Boolean = false,
) {
    /**
     * Builds and returns a browser menu with [items]
     */
    open fun build(context: Context): BrowserMenu {
        val adapter = BrowserMenuAdapter(context, items)
        return BrowserMenu(adapter)
    }
}
