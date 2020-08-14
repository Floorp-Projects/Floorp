/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.tabstray.ext

import mozilla.components.browser.tabstray.TabsAdapter
import mozilla.components.concept.tabstray.Tab
import mozilla.components.concept.tabstray.TabsTray

/**
 * Performs the given action when the [TabsTray.Observer.onTabsUpdated] is invoked.
 *
 * The action will only be invoked once and then removed.
 */
inline fun TabsAdapter.doOnTabsUpdated(crossinline action: () -> Unit) {
    register(object : TabsTray.Observer {
        override fun onTabsUpdated() {
            unregister(this)
            action()
        }

        override fun onTabSelected(tab: Tab) = Unit
        override fun onTabClosed(tab: Tab) = Unit
    })
}
