/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.syncedtabs.ext

import mozilla.components.browser.storage.sync.TabEntry
import mozilla.components.feature.syncedtabs.ClientTabPair
import mozilla.components.feature.syncedtabs.storage.SyncedTabsStorage

/**
 * Get all the synced tabs that match the optional filter.
 *
 * @param limit How many synced tabs to query. A negative value will query all tabs. Defaults to `-1`.
 * @param filter Optional filter for the active [TabEntry] of each tab.
 */
internal suspend fun SyncedTabsStorage.getActiveDeviceTabs(
    limit: Int = -1,
    filter: (TabEntry) -> Boolean = { true },
): List<ClientTabPair> {
    if (limit == 0) return emptyList()

    return getSyncedDeviceTabs().fold(mutableListOf()) { result, (client, tabs) ->
        tabs.forEach { tab ->
            val activeTabEntry = tab.active()
            if (filter(activeTabEntry)) {
                result.add(
                    ClientTabPair(
                        clientName = client.displayName,
                        tab = activeTabEntry,
                        lastUsed = tab.lastUsed,
                        deviceType = client.deviceType,
                    ),
                )

                if (result.size == limit) {
                    return result
                }
            }
        }
        result
    }
}
