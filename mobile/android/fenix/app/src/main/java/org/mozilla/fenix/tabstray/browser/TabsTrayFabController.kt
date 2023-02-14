/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

/**
 * Contract for handling all user interactions with the Tabs Tray floating action button.
 */
interface TabsTrayFabController {
    /**
     * Opens a new normal tab.
     */
    fun handleNormalTabsFabClick()

    /**
     * Opens a new private tab.
     */
    fun handlePrivateTabsFabClick()

    /**
     * Starts a re-sync of synced content if a sync isn't already underway.
     */
    fun handleSyncedTabsFabClick()
}
