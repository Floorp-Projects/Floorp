/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.search

import kotlinx.coroutines.experimental.runBlocking
import org.json.JSONArray
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.Assert.assertEquals
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class SearchSuggestionClientTest {
    @Test
    fun `Get a list of results based on a search engine`() {
        val searchEngine = SearchEngineParser().load(
                RuntimeEnvironment.application.assets,
                "google", "searchplugins/google-nocodes.xml")

        val client = SearchSuggestionClient(searchEngine, { "[\"firefox\",[\"firefox\",\"firefox for mac\",\"firefox quantum\",\"firefox update\",\"firefox esr\",\"firefox focus\",\"firefox addons\",\"firefox extensions\",\"firefox nightly\",\"firefox clear cache\"]]" } )

        runBlocking {
            val results = client.getSuggestions("firefox")
            val expectedResults = listOf("firefox", "firefox for mac", "firefox quantum", "firefox update", "firefox esr", "firefox focus", "firefox addons", "firefox extensions", "firefox nightly", "firefox clear cache")

            assertEquals(expectedResults, results)
        }
    }
}
