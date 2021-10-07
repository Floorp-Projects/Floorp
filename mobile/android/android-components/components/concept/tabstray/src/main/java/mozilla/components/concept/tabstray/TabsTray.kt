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
    @Deprecated(
        "We no longer support diff updates. Use alternatives like DiffUtil and ListAdapter directly.",
        ReplaceWith("Unit")
    )
    fun onTabsInserted(position: Int, count: Int) = Unit

    /**
     * Called after updateTabs() when <code>count</code> number of tabs are removed from
     * the given position.
     */
    @Deprecated(
        "We no longer support diff updates. Use alternatives like DiffUtil and ListAdapter directly.",
        ReplaceWith("Unit")
    )
    fun onTabsRemoved(position: Int, count: Int) = Unit

    /**
     * Called after updateTabs() when a tab changes it position.
     */
    @Deprecated(
        "We no longer support diff updates. Use alternatives like DiffUtil and ListAdapter directly.",
        ReplaceWith("Unit")
    )
    fun onTabsMoved(fromPosition: Int, toPosition: Int) = Unit

    /**
     * Called after updateTabs() when <code>count</code> number of tabs are updated at the
     * given position.
     */
    @Deprecated(
        "We no longer support diff updates. Use alternatives like DiffUtil and ListAdapter directly.",
        ReplaceWith("Unit")
    )
    fun onTabsChanged(position: Int, count: Int) = Unit

    /**
     * Called when binding a new item to get if it should be shown as selected or not.
     */
    fun isTabSelected(tabs: Tabs, position: Int): Boolean

    /**
     * Convenience method to cast the implementation of this interface to an Android View object.
     */
    @Deprecated("Will be removed in a future release.", ReplaceWith(""))
    fun asView(): View = this as View
}
