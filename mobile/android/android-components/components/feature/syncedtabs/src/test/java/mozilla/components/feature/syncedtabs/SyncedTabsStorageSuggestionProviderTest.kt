/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.syncedtabs

import android.graphics.drawable.Drawable
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.awesomebar.AwesomeBar.Suggestion.Flag
import mozilla.components.feature.syncedtabs.helper.getDevice1Tabs
import mozilla.components.feature.syncedtabs.helper.getDevice2Tabs
import mozilla.components.feature.syncedtabs.storage.SyncedTabsStorage
import mozilla.components.support.ktx.kotlin.tryGetHostFromUrl
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Before
import org.junit.Test

class SyncedTabsStorageSuggestionProviderTest {
    private lateinit var syncedTabs: SyncedTabsStorage
    private lateinit var indicatorIcon: DeviceIndicators
    private lateinit var indicatorIconDesktop: Drawable
    private lateinit var indicatorIconMobile: Drawable

    @Before
    fun setup() {
        syncedTabs = mock()
        indicatorIcon = mock()
        indicatorIconDesktop = mock()
        indicatorIconMobile = mock()
    }

    @Test
    fun `matches remote tabs`() = runTest {
        val provider = SyncedTabsStorageSuggestionProvider(syncedTabs, mock(), mock(), indicatorIcon)
        val deviceTabs1 = getDevice1Tabs()
        val deviceTabs2 = getDevice2Tabs()
        whenever(syncedTabs.getSyncedDeviceTabs()).thenReturn(listOf(deviceTabs1, deviceTabs2))
        whenever(indicatorIcon.desktop).thenReturn(indicatorIconDesktop)
        whenever(indicatorIcon.mobile).thenReturn(indicatorIconMobile)

        val suggestions = provider.onInputChanged("bobo")
        assertEquals(3, suggestions.size)
        assertEquals("Hello Bobo", suggestions[0].title)
        assertEquals("Foo Client", suggestions[0].description)
        assertEquals("In URL", suggestions[1].title)
        assertEquals("Foo Client", suggestions[1].description)
        assertEquals("BOBO in CAPS", suggestions[2].title)
        assertEquals("Bar Client", suggestions[2].description)
        assertEquals(setOf(Flag.SYNC_TAB), suggestions[0].flags)
        assertEquals(setOf(Flag.SYNC_TAB), suggestions[1].flags)
        assertEquals(setOf(Flag.SYNC_TAB), suggestions[2].flags)
        assertEquals(indicatorIconDesktop, suggestions[0].indicatorIcon)
        assertEquals(indicatorIconDesktop, suggestions[1].indicatorIcon)
        assertEquals(indicatorIconMobile, suggestions[2].indicatorIcon)
        assertNotNull(suggestions[0].indicatorIcon)
        assertNotNull(suggestions[1].indicatorIcon)
        assertNotNull(suggestions[2].indicatorIcon)
    }

    @Test
    fun `GIVEN an external filter WHEN querying tabs THEN return only the results that pass through the filter`() = runTest {
        val deviceTabs1 = getDevice1Tabs()
        val deviceTabs2 = getDevice2Tabs()
        whenever(syncedTabs.getSyncedDeviceTabs()).thenReturn(listOf(deviceTabs1, deviceTabs2))
        whenever(indicatorIcon.desktop).thenReturn(indicatorIconDesktop)
        whenever(indicatorIcon.mobile).thenReturn(indicatorIconMobile)

        val provider = SyncedTabsStorageSuggestionProvider(
            syncedTabs = syncedTabs,
            loadUrlUseCase = mock(),
            icons = mock(),
            deviceIndicators = indicatorIcon,
            resultsUrlFilter = {
                it.tryGetHostFromUrl() == "https://foo.bar".tryGetHostFromUrl()
            },
        )

        val suggestions = provider.onInputChanged("foo")

        assertEquals(2, suggestions.size)
        // The url is behind the "onSuggestionClicked" lambda.
        // Check the descriptions of the only two tabs that have the "foo.bar" host.
        assertEquals(2, suggestions.map { it.description }.filter { it == "Foo Client" }.size)
    }
}
