/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.awesomebar.provider

import android.content.Context
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.feature.search.ext.createSearchEngine
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@ExperimentalCoroutinesApi // for runTest
@RunWith(AndroidJUnit4::class)
class SearchEngineSuggestionProviderTest {
    private lateinit var defaultProvider: SearchEngineSuggestionProvider
    private val engineList = listOf(
        createSearchEngine("amazon", "https://www.amazon.org/?q={searchTerms}", mock()),
        createSearchEngine("bing", "https://www.bing.com/?q={searchTerms}", mock()),
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
    fun `Provider returns a match when list contains the typed engine`() = runTest {
        val suggestions = defaultProvider.onInputChanged("am")

        assertEquals("Search amazon", suggestions[0].title)
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
        val suggestions = defaultProvider.onInputChanged("n")

        assertEquals(defaultProvider.maxSuggestions, suggestions.size)
    }
}
