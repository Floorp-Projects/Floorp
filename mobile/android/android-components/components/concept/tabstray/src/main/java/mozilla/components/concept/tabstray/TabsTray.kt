/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.tabstray

import android.view.View
import mozilla.components.support.base.observer.Observable

/**
 * Generic interface for components that provide "tabs tray" functionality.
 */
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
     *
     * Calling this method is usually followed by calling onTabs*() methods to indicate what
     * exactly has changed. This allows the tabs tray implementation to animate between the old and
     * new state.
     */
    fun updateTabs(tabs: Tabs)

    /**
     * Called after updateTabs() when <code>count</code> number of tabs are inserted at the
     * given position.
     */
    fun onTabsInserted(position: Int, count: Int)

    /**
     * Called after updateTabs() when <code>count</code> number of tabs are removed from
     * the given position.
     */
    fun onTabsRemoved(position: Int, count: Int)

    /**
     * Called after updateTabs() when a tab changes it position.
     */
    fun onTabsMoved(fromPosition: Int, toPosition: Int)

    /**
     * Called after updateTabs() when <code>count</code> number of tabs are updated at the
     * given position.
     */
    fun onTabsChanged(position: Int, count: Int)

    /**
     * Convenience method to cast the implementation of this interface to an Android View object.
     */
    fun asView(): View = this as View
}
