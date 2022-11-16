/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package mozilla.components.feature.syncedtabs

import mozilla.components.browser.storage.sync.TabEntry
import mozilla.components.concept.sync.DeviceType

/**
 * Mapping of a device and the active [TabEntry] for each synced tab.
 */
internal data class ClientTabPair(
    val clientName: String,
    val tab: TabEntry,
    val lastUsed: Long,
    val deviceType: DeviceType,
)
