/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.ext

import mozilla.components.browser.storage.sync.SyncedDeviceTabs
import mozilla.components.support.ktx.kotlin.trimmed
import org.mozilla.fenix.tabstray.syncedtabs.SyncedTabsListItem

/**
 * Converts a list of [SyncedDeviceTabs] into a list of [SyncedTabsListItem].
 */
fun List<SyncedDeviceTabs>.toComposeList(): List<SyncedTabsListItem> = asSequence().flatMap { (device, tabs) ->
    val deviceTabs = if (tabs.isEmpty()) {
        emptyList()
    } else {
        tabs.map {
            val url = it.active().url
            val titleText = it.active().title.ifEmpty { url.trimmed() }
            SyncedTabsListItem.Tab(titleText, url, it)
        }
    }

    sequenceOf(SyncedTabsListItem.DeviceSection(device.displayName, deviceTabs))
}.toList()
