/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.storage

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class CustomSearchEngineStorageTest {
    @Test
    fun `saveSearchEngine successfully saves`() = runTest {
        val searchEngine = SearchEngine(
            id = "id1",
            name = "example",
            icon = mock(),
            type = SearchEngine.Type.CUSTOM,
            resultUrls = listOf("https://www.example.com/search"),
        )

        val storage = CustomSearchEngineStorage(testContext, coroutineContext)
        assertTrue(storage.saveSearchEngine(searchEngine))
        assertTrue(storage.getSearchFile(searchEngine.id).baseFile.exists())
    }

    @Test
    fun `loadSearchEngine successfully loads after saving`() = runTest {
        val searchEngine = SearchEngine(
            id = "id1",
            name = "example",
            icon = mock(),
            type = SearchEngine.Type.CUSTOM,
            resultUrls = listOf("https://www.example.com/search"),
        )

        val storage = CustomSearchEngineStorage(testContext, coroutineContext)
        assertTrue(storage.saveSearchEngine(searchEngine))

        val storedSearchEngine = storage.loadSearchEngine(searchEngine.id)
        assertEquals(searchEngine.id, storedSearchEngine.id)
        assertEquals(searchEngine.name, storedSearchEngine.name)
        assertEquals(searchEngine.type, storedSearchEngine.type)
        assertEquals(searchEngine.resultUrls, storedSearchEngine.resultUrls)
    }

    @Test
    @Ignore("https://github.com/mozilla-mobile/android-components/issues/8124")
    fun `loadSearchEngineList successfully loads after saving`() = runTest {
        val searchEngine = SearchEngine(
            id = "id1",
            name = "example",
            icon = mock(),
            type = SearchEngine.Type.CUSTOM,
            resultUrls = listOf("https://www.example.com/search"),
        )
        val searchEngineTwo = SearchEngine(
            id = "id2",
            name = "searchTwo",
            icon = mock(),
            type = SearchEngine.Type.CUSTOM,
            resultUrls = listOf("https://www.searchtwo.com/search"),
        )

        val storage = CustomSearchEngineStorage(testContext, coroutineContext)
        assertTrue(storage.saveSearchEngine(searchEngine))
        assertTrue(storage.saveSearchEngine(searchEngineTwo))

        val storedSearchEngines = storage.loadSearchEngineList()
        assertEquals(2, storedSearchEngines.size)
        assertEquals(searchEngine.id, storedSearchEngines[0].id)
        assertEquals(searchEngine.name, storedSearchEngines[0].name)
        assertEquals(searchEngine.type, storedSearchEngines[0].type)
        assertEquals(searchEngine.resultUrls, storedSearchEngines[0].resultUrls)
        assertEquals(searchEngineTwo.id, storedSearchEngines[1].id)
        assertEquals(searchEngineTwo.name, storedSearchEngines[1].name)
        assertEquals(searchEngineTwo.type, storedSearchEngines[1].type)
        assertEquals(searchEngineTwo.resultUrls, storedSearchEngines[1].resultUrls)
    }

    @Test
    fun `removeSearchEngine successfully deletes`() = runTest {
        val searchEngine = SearchEngine(
            id = "id1",
            name = "example",
            icon = mock(),
            type = SearchEngine.Type.CUSTOM,
            resultUrls = listOf("https://www.example.com/search"),
        )

        val storage = CustomSearchEngineStorage(testContext, coroutineContext)
        assertTrue(storage.saveSearchEngine(searchEngine))

        storage.removeSearchEngine(searchEngine.id)
        assertTrue(storage.loadSearchEngineList().isEmpty())
    }
}
