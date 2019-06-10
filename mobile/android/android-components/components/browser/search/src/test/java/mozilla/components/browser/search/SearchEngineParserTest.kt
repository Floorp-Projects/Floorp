/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.search

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import java.io.IOException

@RunWith(AndroidJUnit4::class)
class SearchEngineParserTest {

    @Test
    fun `SearchEngine can be parsed from assets`() {
        val searchEngine = SearchEngineParser().load(
            testContext.assets,
                "google", "searchplugins/google-b-m.xml")

        assertEquals("google", searchEngine.identifier)
        assertEquals("Google", searchEngine.name)
        assertNotNull(searchEngine.icon)

        val url = searchEngine.buildSearchUrl("HelloWorld")
        assertEquals("https://www.google.com/search?q=HelloWorld&ie=utf-8&oe=utf-8&client=firefox-b-m", url)

        val suggestionURL = searchEngine.buildSuggestionsURL("Mozilla")
        assertEquals("https://www.google.com/complete/search?client=firefox&q=Mozilla", suggestionURL)
    }

    @Test
    fun `SearchEngine without SuggestUri wont build suggestionURL`() {
        val searchEngine = SearchEngineParser().load(
            testContext.assets,
                "amazon", "searchplugins/amazon-au.xml")

        val suggestionURL = searchEngine.buildSuggestionsURL("Mozilla")
        assertNull(suggestionURL)
    }

    @Test(expected = IOException::class)
    fun `Parsing not existing file will throw exception`() {
        SearchEngineParser().load(
            testContext.assets,
                "google", "does/not/exist.xml")
    }
}
