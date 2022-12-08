/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.storage

import android.util.AtomicFile
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File
import java.io.IOException

@RunWith(AndroidJUnit4::class)
class SearchEngineReaderTest {
    @Test
    fun `SearchEngineReader can read from a file`() {
        val searchEngine = SearchEngine(
            id = "id1",
            name = "example",
            icon = mock(),
            type = SearchEngine.Type.CUSTOM,
            resultUrls = listOf("https://www.example.com/search"),
        )
        val readSearchEngine = saveAndLoadSearchEngine(searchEngine)

        assertEquals(searchEngine.id, readSearchEngine.id)
        assertEquals(searchEngine.name, readSearchEngine.name)
        assertEquals(searchEngine.type, readSearchEngine.type)
        assertEquals(searchEngine.resultUrls, readSearchEngine.resultUrls)
        assertTrue(readSearchEngine.isGeneral)
    }

    @Test(expected = IOException::class)
    fun `Parsing not existing file will throw exception`() {
        val searchEngine = SearchEngine(
            id = "id1",
            name = "example",
            icon = mock(),
            type = SearchEngine.Type.CUSTOM,
            resultUrls = listOf("https://www.example.com/search"),
        )
        val reader = SearchEngineReader(type = SearchEngine.Type.CUSTOM)
        val invalidFile = AtomicFile(File("", ""))
        reader.loadFile(searchEngine.id, invalidFile)
    }

    @Test
    fun `WHEN SearchEngineReader is loading bundled search engines from a file THEN the correct SearchEngine properties are parsed`() {
        for (id in GENERAL_SEARCH_ENGINE_IDS + setOf("mozilla", "wikipedia")) {
            val searchEngine = SearchEngine(
                id = id,
                name = "example",
                icon = mock(),
                type = SearchEngine.Type.BUNDLED,
                resultUrls = listOf("https://www.example.com/search"),
            )
            val readSearchEngine = saveAndLoadSearchEngine(searchEngine)

            assertEquals(searchEngine.id, readSearchEngine.id)
            assertEquals(searchEngine.name, readSearchEngine.name)
            assertEquals(searchEngine.type, readSearchEngine.type)
            assertEquals(searchEngine.resultUrls, readSearchEngine.resultUrls)
            assertEquals(id in GENERAL_SEARCH_ENGINE_IDS, readSearchEngine.isGeneral)
        }
    }
    private fun saveAndLoadSearchEngine(searchEngine: SearchEngine): SearchEngine {
        val storage = CustomSearchEngineStorage(testContext)
        val writer = SearchEngineWriter()
        val reader = SearchEngineReader(type = searchEngine.type)
        val file = storage.getSearchFile(searchEngine.id)

        writer.saveSearchEngineXML(searchEngine, file)

        return reader.loadFile(searchEngine.id, file)
    }
}
