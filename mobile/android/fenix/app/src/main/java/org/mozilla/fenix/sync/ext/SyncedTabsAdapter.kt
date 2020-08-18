/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.sync.ext

import mozilla.components.browser.storage.sync.SyncedDeviceTabs
import org.mozilla.fenix.sync.SyncedTabsAdapter.AdapterItem

fun List<SyncedDeviceTabs>.toAdapterList(
): MutableList<AdapterItem> {
    val allDeviceTabs = mutableListOf<AdapterItem>()

    forEach { (device, tabs) ->
        if (tabs.isNotEmpty()) {
            allDeviceTabs.add(AdapterItem.Device(device))
            tabs.mapTo(allDeviceTabs) { AdapterItem.Tab(it) }
        }
    }

    return allDeviceTabs
}