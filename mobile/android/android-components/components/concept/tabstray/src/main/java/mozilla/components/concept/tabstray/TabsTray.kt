/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.tabstray

import mozilla.components.support.base.observer.Observable

/**
 * Generic interface for components that provide "tabs tray" functionality.
 */
@Deprecated("This will be removed in a future release", ReplaceWith("TabsTray", "mozilla.components.browser.tabstray"))
@Suppress("Deprecation")
interface TabsTray : Observable<TabsTray.Observer> {
    /**
     * Interface to be implemented by classes that want to observe a tabs tray.
     */
    interface Observer {
        /**
         * One or many tabs have been added or removed.
         */
        fun onTabsUpdated() = Unit

        /**
         * A new tab has been selected.
         */
        fun onTabSelected(tab: Tab)

        /**
         * A tab has been closed.
         */
        fun onTabClosed(tab: Tab)
    }

    /**
     * Updates the list of tabs.
     */
    fun updateTabs(tabs: Tabs)

    /**
     * Called when binding a new item to get if it should be shown as selected or not.
     */
    fun isTabSelected(tabs: Tabs, position: Int): Boolean
}
