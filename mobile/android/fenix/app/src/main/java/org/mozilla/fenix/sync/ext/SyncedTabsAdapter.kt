/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.sync.ext

import mozilla.components.browser.storage.sync.SyncedDeviceTabs
import org.mozilla.fenix.sync.SyncedTabsAdapter.AdapterItem

/**
 * Converts a list of [SyncedDeviceTabs] into a list of [AdapterItem].
 */
fun List<SyncedDeviceTabs>.toAdapterList() = asSequence().flatMap { (device, tabs) ->

    val deviceTabs = if (tabs.isEmpty()) {
        sequenceOf(AdapterItem.NoTabs(device))
    } else {
        tabs.asSequence().map { AdapterItem.Tab(it) }
    }

    sequenceOf(AdapterItem.Device(device)) + deviceTabs
}.toList()
