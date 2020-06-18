/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.syncedtabs

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.storage.sync.SyncedDeviceTabs
import mozilla.components.browser.storage.sync.Tab
import mozilla.components.browser.storage.sync.TabEntry
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceType
import mozilla.components.feature.syncedtabs.storage.SyncedTabsStorage
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class SyncedTabsStorageSuggestionProviderTest {
    private lateinit var syncedTabs: SyncedTabsStorage

    @Before
    fun setup() {
        syncedTabs = mock()
    }

    @Test
    fun `matches remote tabs`() = runBlocking {
        val provider = SyncedTabsStorageSuggestionProvider(syncedTabs, mock())
        val deviceTabs1 = SyncedDeviceTabs(
            Device(
                id = "client1",
                displayName = "Foo Client",
                deviceType = DeviceType.DESKTOP,
                isCurrentDevice = false,
                lastAccessTime = null,
                capabilities = listOf(),
                subscriptionExpired = false,
                subscription = null
            ),
            listOf(
                Tab(
                    listOf(
                        TabEntry("Foo", "https://foo.bar", null), /* active tab */
                        TabEntry("Bobo", "https://foo.bar", null),
                        TabEntry("Foo", "https://bobo.bar", null)
                    ), 0, 1
                ),
                Tab(
                    listOf(
                        TabEntry("Hello Bobo", "https://foo.bar", null) /* active tab */
                    ), 0, 5
                ),
                Tab(
                    listOf(
                        TabEntry("In URL", "https://bobo.bar", null) /* active tab */
                    ), 0, 2
                )
            )
        )
        val deviceTabs2 = SyncedDeviceTabs(
            Device(
                id = "client2",
                displayName = "Bar Client",
                deviceType = DeviceType.MOBILE,
                isCurrentDevice = false,
                lastAccessTime = null,
                capabilities = listOf(),
                subscriptionExpired = false,
                subscription = null
            ),
            listOf(
                Tab(
                    listOf(
                        TabEntry("Bar", "https://bar.bar", null),
                        TabEntry("BOBO in CAPS", "https://obob.bar", null) /* active tab */
                    ), 1, 1
                )
            )
        )
        whenever(syncedTabs.getSyncedDeviceTabs()).thenReturn(listOf(deviceTabs1, deviceTabs2))

        val suggestions = provider.onInputChanged("bobo")
        assertEquals(3, suggestions.size)
        assertEquals("Hello Bobo", suggestions[0].title)
        assertEquals("Foo Client", suggestions[0].description)
        assertEquals("In URL", suggestions[1].title)
        assertEquals("Foo Client", suggestions[1].description)
        assertEquals("BOBO in CAPS", suggestions[2].title)
        assertEquals("Bar Client", suggestions[2].description)
    }
}
