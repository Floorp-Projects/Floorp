/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.syncedtabs

import mozilla.components.browser.storage.sync.Tab as SyncTab

/**
 * The various types of list items that can be found in a [SyncedTabsList].
 */
sealed class SyncedTabsListItem {

    /**
     * A device header for displaying a synced device.
     *
     * @property displayName The user's custom name of their synced device.
     */
    data class Device(val displayName: String) : SyncedTabsListItem()

    /**
     * A section for displaying a synced device and its tabs.
     *
     * @property displayName The user's custom name of their synced device.
     * @property tabs The user's tabs from their synced device.
     */
    data class DeviceSection(val displayName: String, val tabs: List<Tab>) : SyncedTabsListItem()

    /**
     * A tab that was synced.
     *
     * @property displayTitle The title of the tab's web page.
     * @property displayURL The tab's URL up to BrowserToolbar.MAX_URI_LENGTH characters long.
     * @property tab The underlying SyncTab object passed when the tab is clicked.
     */
    data class Tab(
        val displayTitle: String,
        val displayURL: String,
        val tab: SyncTab,
    ) : SyncedTabsListItem()

    /**
     * A placeholder for a device that has no tabs synced.
     */
    object NoTabs : SyncedTabsListItem()

    /**
     * A message displayed if an error was encountered.
     *
     * @property errorText The text to be displayed to the user.
     * @property errorButton Optional class to set up and handle any clicks in the Error UI.
     */
    data class Error(
        val errorText: String,
        val errorButton: ErrorButton? = null,
    ) : SyncedTabsListItem()

    /**
     * A button displayed if an error has optional interaction.
     *
     * @property buttonText The error button's text and accessibility hint.
     * @property onClick Lambda called when the button is clicked.
     */
    data class ErrorButton(
        val buttonText: String,
        val onClick: () -> Unit,
    )
}
