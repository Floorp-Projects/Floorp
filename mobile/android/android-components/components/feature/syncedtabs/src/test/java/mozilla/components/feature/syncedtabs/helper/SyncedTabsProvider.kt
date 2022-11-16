/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.syncedtabs.helper

import mozilla.components.browser.storage.sync.SyncedDeviceTabs
import mozilla.components.browser.storage.sync.Tab
import mozilla.components.browser.storage.sync.TabEntry
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceType.DESKTOP
import mozilla.components.concept.sync.DeviceType.MOBILE

/**
 * Get fake tabs from a fake desktop device.
 */
internal fun getDevice1Tabs() = SyncedDeviceTabs(
    Device(
        id = "client1",
        displayName = "Foo Client",
        deviceType = DESKTOP,
        isCurrentDevice = false,
        lastAccessTime = null,
        capabilities = listOf(),
        subscriptionExpired = false,
        subscription = null,
    ),
    listOf(
        Tab(
            listOf(
                TabEntry("Foo", "https://foo.bar", null), /* active tab */
                TabEntry("Bobo", "https://foo.bar", null),
                TabEntry("Foo", "https://bobo.bar", null),
            ),
            0,
            1,
        ),
        Tab(
            listOf(
                TabEntry("Hello Bobo", "https://foo.bar", null), /* active tab */
            ),
            0,
            5,
        ),
        Tab(
            listOf(
                TabEntry("In URL", "https://bobo.bar", null), /* active tab */
            ),
            0,
            2,
        ),
    ),
)

/**
 * Get fake tabs from a fake mobile device.
 */
internal fun getDevice2Tabs() = SyncedDeviceTabs(
    Device(
        id = "client2",
        displayName = "Bar Client",
        deviceType = MOBILE,
        isCurrentDevice = false,
        lastAccessTime = null,
        capabilities = listOf(),
        subscriptionExpired = false,
        subscription = null,
    ),
    listOf(
        Tab(
            listOf(
                TabEntry("Bar", "https://bar.bar", null),
                TabEntry("BOBO in CAPS", "https://obob.bar", null), /* active tab */
            ),
            1,
            1,
        ),
    ),
)
