/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import android.content.Context
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.feature.search.ext.createSearchEngine
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test

@ExperimentalCoroutinesApi // for runTest

class SearchEngineSuggestionProviderTest {
    private lateinit var defaultProvider: SearchEngineSuggestionProvider
    private val engineList = listOf(
        createSearchEngine("amazon", "https://www.amazon.org/?q={searchTerms}", mock()),
        createSearchEngine("bing", "https://www.bing.com/?q={searchTerms}", mock()),
        createSearchEngine("bingo", "https://www.bingo.com/?q={searchTerms}", mock()),
    )
    private val testContext: Context = mock()

    @Before
    fun setup() {
        defaultProvider = SearchEngineSuggestionProvider(
            testContext,
            engineList,
            mock(),
            1,
            "description",
            mock(),
            maxSuggestions = 1,
            charactersThreshold = 1,
        )

        whenever(testContext.getString(1, "amazon")).thenReturn("Search amazon")
        whenever(testContext.getString(1, "bing")).thenReturn("Search bing")
        whenever(testContext.getString(1, "bingo")).thenReturn("Search bingo")
    }

    @Test
    fun `Provider returns empty list when text is empty`() = runTest {
        val suggestions = defaultProvider.onInputChanged("")

        assertTrue(suggestions.isEmpty())
    }

    @Test
    fun `Provider returns empty list when text is blank`() = runTest {
        val suggestions = defaultProvider.onInputChanged("  ")

        assertTrue(suggestions.isEmpty())
    }

    @Test
    fun `Provider returns empty list when text is shorter than charactersThreshold`() = runTest {
        val provider = SearchEngineSuggestionProvider(
            testContext,
            engineList,
            mock(),
            1,
            "description",
            mock(),
            charactersThreshold = 3,
        )

        val suggestions = provider.onInputChanged("am")

        assertTrue(suggestions.isEmpty())
    }

    @Test
    fun `Provider returns empty list when list does not contain engines with typed text`() = runTest {
        val suggestions = defaultProvider.onInputChanged("x")

        assertTrue(suggestions.isEmpty())
    }

    @Test
    fun `WHEN input matches the beginning of the engine name THEN return the corresponding engine`() = runTest {
        val suggestions = defaultProvider.onInputChanged("am")

        assertEquals("Search amazon", suggestions[0].title)
    }

    @Test
    fun `WHEN input matches not the beginning of the engine name THEN return nothing`() = runTest {
        val suggestions = defaultProvider.onInputChanged("ma")

        assertEquals(0, suggestions.size)
    }

    @Test
    fun `Provider returns empty list when the engine list is empty`() = runTest {
        val providerEmpty = SearchEngineSuggestionProvider(
            testContext,
            emptyList(),
            mock(),
            1,
            "description",
            mock(),
        )

        val suggestions = providerEmpty.onInputChanged("a")

        assertTrue(suggestions.isEmpty())
    }

    @Test
    fun `Provider limits number of returned suggestions to maxSuggestions`() = runTest {
        // this should match to both engines in list
        val suggestions = defaultProvider.onInputChanged("bi")

        assertEquals(defaultProvider.maxSuggestions, suggestions.size)
    }
}
