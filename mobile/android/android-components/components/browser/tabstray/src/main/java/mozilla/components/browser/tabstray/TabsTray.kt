/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.tabstray

import mozilla.components.browser.state.state.TabPartition
import mozilla.components.browser.state.state.TabSessionState

/**
 * An interface to display a list of tabs.
 */
interface TabsTray {

    /**
     * Interface to be implemented by classes that want to observe or react to the interactions on the tabs list.
     */
    interface Delegate {

        /**
         * A new tab has been selected.
         */
        fun onTabSelected(tab: TabSessionState, source: String? = null)

        /**
         * A tab has been closed.
         */
        fun onTabClosed(tab: TabSessionState, source: String? = null)
    }

    /**
     * Called when the list of tabs are updated.
     */
    fun updateTabs(tabs: List<TabSessionState>, tabPartition: TabPartition?, selectedTabId: String?)
}
