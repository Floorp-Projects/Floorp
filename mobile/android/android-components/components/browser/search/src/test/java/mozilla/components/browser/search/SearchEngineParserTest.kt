/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.search

import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import java.io.IOException

@RunWith(RobolectricTestRunner::class)
class SearchEngineParserTest {
    @Test
    fun `SearchEngine can be parsed from assets`() {
        val searchEngine = SearchEngineParser().load(
                RuntimeEnvironment.application.assets,
                "google", "searchplugins/google-nocodes.xml")

        assertEquals("google", searchEngine.identifier)
        assertEquals("Google", searchEngine.name)
        assertNotNull(searchEngine.icon)

        val url = searchEngine.buildSearchUrl("HelloWorld")
        assertEquals("https://www.google.com/search?q=HelloWorld&ie=utf-8&oe=utf-8", url)

        val suggestionURL = searchEngine.buildSuggestionsURL("Mozilla")
        assertEquals("https://www.google.com/complete/search?client=firefox&q=Mozilla", suggestionURL)
    }

    @Test
    fun `SearchEngine without SuggestUri wont build suggestionURL`() {
        val searchEngine = SearchEngineParser().load(
                RuntimeEnvironment.application.assets,
                "amazon", "searchplugins/amazon-au.xml")

        val suggestionURL = searchEngine.buildSuggestionsURL("Mozilla")
        assertNull(suggestionURL)
    }

    @Test(expected = IOException::class)
    fun `Parsing not existing file will throw exception`() {
        SearchEngineParser().load(
                RuntimeEnvironment.application.assets,
                "google", "does/not/exist.xml")
    }
}
