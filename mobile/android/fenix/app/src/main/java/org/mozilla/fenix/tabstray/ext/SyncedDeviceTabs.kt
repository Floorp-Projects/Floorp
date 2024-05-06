/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.ext

import mozilla.components.browser.storage.sync.SyncedDeviceTabs
import mozilla.components.concept.sync.DeviceCapability
import mozilla.components.support.ktx.kotlin.trimmed
import org.mozilla.fenix.tabstray.syncedtabs.SyncedTabsListItem
import org.mozilla.fenix.tabstray.syncedtabs.SyncedTabsListSupportedFeature

/**
 * Converts a list of [SyncedDeviceTabs] into a list of [SyncedTabsListItem].
 *
 * @param features Supported [SyncedTabsListSupportedFeature]s.
 */
fun List<SyncedDeviceTabs>.toComposeList(
    features: Set<SyncedTabsListSupportedFeature> = emptySet(),
): List<SyncedTabsListItem> =
    asSequence().flatMap { (device, tabs) ->
        val deviceTabs = if (tabs.isEmpty()) {
            emptyList()
        } else {
            tabs.map {
                val url = it.active().url
                val titleText = it.active().title.ifEmpty { url.trimmed() }
                SyncedTabsListItem.Tab(
                    displayTitle = titleText,
                    displayURL = url,
                    action = if (
                        features.contains(SyncedTabsListSupportedFeature.CLOSE_TABS) &&
                        device.capabilities.contains(DeviceCapability.CLOSE_TABS)
                    ) {
                        SyncedTabsListItem.Tab.Action.Close(deviceId = device.id)
                    } else {
                        SyncedTabsListItem.Tab.Action.None
                    },
                    tab = it,
                )
            }
        }

        sequenceOf(SyncedTabsListItem.DeviceSection(device.displayName, deviceTabs))
    }.toList()
