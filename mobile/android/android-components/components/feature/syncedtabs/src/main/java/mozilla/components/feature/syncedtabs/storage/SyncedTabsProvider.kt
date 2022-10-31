/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.syncedtabs.storage

import mozilla.components.browser.storage.sync.SyncedDeviceTabs

/**
 * Provides tabs from remote Firefox Sync devices.
 */
interface SyncedTabsProvider {

    /**
     * A list of [SyncedDeviceTabs], each containing a synced device and its current tabs.
     */
    suspend fun getSyncedDeviceTabs(): List<SyncedDeviceTabs>
}
