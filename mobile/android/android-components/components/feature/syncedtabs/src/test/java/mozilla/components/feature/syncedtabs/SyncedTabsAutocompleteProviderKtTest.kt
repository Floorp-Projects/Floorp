/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.syncedtabs

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.storage.sync.SyncedDeviceTabs
import mozilla.components.feature.syncedtabs.helper.getDevice1Tabs
import mozilla.components.feature.syncedtabs.helper.getDevice2Tabs
import mozilla.components.feature.syncedtabs.storage.SyncedTabsStorage
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn

@OptIn(ExperimentalCoroutinesApi::class)
@RunWith(AndroidJUnit4::class)
class SyncedTabsAutocompleteProviderKtTest {
    private val syncedTabs: SyncedTabsStorage = mock()

    @Test
    fun `GIVEN synced tabs exist WHEN asked for autocomplete suggestions THEN return the first matching tab`() = runTest {
        val deviceTabs1 = getDevice1Tabs()
        val deviceTabs2 = getDevice2Tabs()
        doReturn(listOf(deviceTabs1, deviceTabs2)).`when`(syncedTabs).getSyncedDeviceTabs()
        val provider = SyncedTabsAutocompleteProvider(syncedTabs)

        var suggestion = provider.getAutocompleteSuggestion("mozilla")
        assertNull(suggestion)

        suggestion = provider.getAutocompleteSuggestion("foo")
        assertNotNull(suggestion)
        assertEquals("foo", suggestion?.input)
        assertEquals("foo.bar", suggestion?.text)
        assertEquals("https://foo.bar", suggestion?.url)
        assertEquals(SYNCED_TABS_AUTOCOMPLETE_SOURCE_NAME, suggestion?.source)
        assertEquals(1, suggestion?.totalItems)

        suggestion = provider.getAutocompleteSuggestion("obob")
        assertNotNull(suggestion)
        assertEquals("obob", suggestion?.input)
        assertEquals("obob.bar", suggestion?.text)
        assertEquals("https://obob.bar", suggestion?.url)
        assertEquals(SYNCED_TABS_AUTOCOMPLETE_SOURCE_NAME, suggestion?.source)
        assertEquals(1, suggestion?.totalItems)
    }

    @Test
    fun `GIVEN open tabs exist WHEN asked for autocomplete suggestions and only private tabs match THEN return null`() = runTest {
        doReturn(emptyList<SyncedDeviceTabs>()).`when`(syncedTabs).getSyncedDeviceTabs()
        val provider = SyncedTabsAutocompleteProvider(syncedTabs)

        var suggestion = provider.getAutocompleteSuggestion("mozilla")
        assertNull(suggestion)

        suggestion = provider.getAutocompleteSuggestion("foo")
        assertNull(suggestion)

        suggestion = provider.getAutocompleteSuggestion("bar")
        assertNull(suggestion)
    }
}
