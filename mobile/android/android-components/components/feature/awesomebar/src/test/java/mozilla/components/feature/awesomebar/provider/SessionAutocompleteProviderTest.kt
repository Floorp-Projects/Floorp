/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith

@OptIn(ExperimentalCoroutinesApi::class)
@RunWith(AndroidJUnit4::class)
class SessionAutocompleteProviderTest {
    @Test
    fun `GIVEN open tabs exist WHEN asked for autocomplete suggestions THEN return the first matching tab`() = runTest {
        val tab1 = createTab("https://allizom.org")
        val tab2 = createTab("https://getpocket.com")
        val tab3 = createTab("https://www.firefox.com")
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(tab1, tab2, tab3),
            ),
        )
        val provider = SessionAutocompleteProvider(store)

        var suggestion = provider.getAutocompleteSuggestion("mozilla")
        assertNull(suggestion)

        suggestion = provider.getAutocompleteSuggestion("all")
        assertNotNull(suggestion)
        assertEquals("all", suggestion?.input)
        assertEquals("allizom.org", suggestion?.text)
        assertEquals("https://allizom.org", suggestion?.url)
        assertEquals(LOCAL_TABS_AUTOCOMPLETE_SOURCE_NAME, suggestion?.source)
        assertEquals(1, suggestion?.totalItems)

        suggestion = provider.getAutocompleteSuggestion("www")
        assertNotNull(suggestion)
        assertEquals("www", suggestion?.input)
        assertEquals("www.firefox.com", suggestion?.text)
        assertEquals("https://www.firefox.com", suggestion?.url)
        assertEquals(LOCAL_TABS_AUTOCOMPLETE_SOURCE_NAME, suggestion?.source)
        assertEquals(1, suggestion?.totalItems)
    }

    @Test
    fun `GIVEN open tabs exist WHEN asked for autocomplete suggestions and only private tabs match THEN return null`() = runTest {
        val tab1 = createTab(url = "https://allizom.org", private = true)
        val tab2 = createTab(url = "https://getpocket.com")
        val tab3 = createTab(url = "https://www.firefox.com", private = true)
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(tab1, tab2, tab3),
            ),
        )
        val provider = SessionAutocompleteProvider(store)

        var suggestion = provider.getAutocompleteSuggestion("mozilla")
        assertNull(suggestion)

        suggestion = provider.getAutocompleteSuggestion("all")
        assertNull(suggestion)

        suggestion = provider.getAutocompleteSuggestion("www")
        assertNull(suggestion)
    }

    @Test
    fun `GIVEN no open tabs exist WHEN asked for autocomplete suggestions THEN return null`() = runTest {
        val provider = SessionAutocompleteProvider(BrowserStore())

        assertNull(provider.getAutocompleteSuggestion("test"))
    }
}
