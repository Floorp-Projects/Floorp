/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.syncedtabs.view

import android.view.View
import mozilla.components.browser.storage.sync.SyncedDeviceTabs
import mozilla.components.browser.storage.sync.Tab

/**
 * An interface for views that can display Firefox Sync "synced tabs" and related UI controls.
 */
interface SyncedTabsView {
    var listener: Listener?

    /**
     * When tab syncing has started.
     */
    fun startLoading() = Unit

    /**
     * When tab syncing has completed.
     */
    fun stopLoading() = Unit

    /**
     * New tabs have been received to display.
     */
    fun displaySyncedTabs(syncedTabs: List<SyncedDeviceTabs>)

    /**
     * An error has occurred that may require various user-interactions based on the [ErrorType].
     */
    fun onError(error: ErrorType)

    /**
     * Casts this [SyncedTabsView] interface to an actual Android [View] object.
     */
    fun asView(): View = (this as View)

    /**
     * An interface for notifying the listener of the [SyncedTabsView].
     */
    interface Listener {

        /**
         * Invoked when a tab has been selected.
         */
        fun onTabClicked(tab: Tab)

        /**
         * Invoked when receiving a request to refresh the synced tabs.
         */
        fun onRefresh()
    }

    /**
     *  The various types of errors that can occur from syncing tabs.
     */
    enum class ErrorType {

        /**
         * Other devices found but there are no tabs to sync.
         * */
        NO_TABS_AVAILABLE,

        /**
         * There are no other devices found with this account and therefore no tabs to sync.
         */
        MULTIPLE_DEVICES_UNAVAILABLE,

        /**
         * The engine for syncing tabs is unavailable. This is mostly due to a user turning off the feature on the
         * Firefox Sync account.
         */
        SYNC_ENGINE_UNAVAILABLE,

        /**
         * There is no Firefox Sync account available. A user needs to sign-in before this feature.
         */
        SYNC_UNAVAILABLE,

        /**
         * The Firefox Sync account requires user-intervention to re-authenticate the account.
         */
        SYNC_NEEDS_REAUTHENTICATION,
    }
}
