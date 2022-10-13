/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.storage

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.search.RegionState
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.util.Locale

@RunWith(AndroidJUnit4::class)
class BundledSearchEnginesStorageTest {
    @Test
    fun `Load search engines for en-US from assets`() = runTest {
        val storage = BundledSearchEnginesStorage(testContext)

        val engines = storage.load(RegionState("US", "US"), Locale("en", "US"))
        val searchEngines = engines.list

        assertEquals(6, searchEngines.size)
    }

    @Test
    fun `Load search engines for all known locales without region`() = runTest {
        val storage = BundledSearchEnginesStorage(testContext)
        val locales = Locale.getAvailableLocales()
        assertTrue(locales.isNotEmpty())

        for (locale in locales) {
            val engines = storage.load(RegionState.Default, locale)
            assertTrue(engines.list.isNotEmpty())
            assertFalse(engines.defaultSearchEngineId.isNullOrEmpty())
        }
    }

    @Test
    fun `Load search engines for de-DE with global US region override`() = runTest {
        // Without region
        run {
            val storage = BundledSearchEnginesStorage(testContext)
            val engines = storage.load(RegionState.Default, Locale("de", "DE"))
            val searchEngines = engines.list

            assertEquals(8, searchEngines.size)
            assertContainsSearchEngine("google-b-m", searchEngines)
            assertContainsNotSearchEngine("google-2018", searchEngines)
        }
        // With region
        run {
            val storage = BundledSearchEnginesStorage(testContext)
            val engines = storage.load(RegionState("US", "US"), Locale("de", "DE"))
            val searchEngines = engines.list

            assertEquals(8, searchEngines.size)
            assertContainsSearchEngine("google-b-1-m", searchEngines)
            assertContainsNotSearchEngine("google", searchEngines)
        }
    }

    @Test
    fun `Load search engines for en-US with local RU region override`() = runTest {
        // Without region
        run {
            val storage = BundledSearchEnginesStorage(testContext)
            val engines = storage.load(RegionState.Default, Locale("en", "US"))
            val searchEngines = engines.list

            println("searchEngines = $searchEngines")
            assertEquals(6, searchEngines.size)
            assertContainsNotSearchEngine("yandex-en", searchEngines)
        }
        // With region
        run {
            val storage = BundledSearchEnginesStorage(testContext)
            val engines = storage.load(RegionState("RU", "RU"), Locale("en", "US"))
            val searchEngines = engines.list

            println("searchEngines = $searchEngines")
            assertEquals(5, searchEngines.size)
            assertContainsSearchEngine("google-com-nocodes", searchEngines)
            assertContainsNotSearchEngine("yandex-en", searchEngines)
        }
    }

    @Test
    fun `Load search engines for zh-CN_CN locale with searchDefault override`() = runTest {
        val storage = BundledSearchEnginesStorage(testContext)
        val engines = storage.load(RegionState("CN", "CN"), Locale("zh", "CN"))
        val searchEngines = engines.list

        // visibleDefaultEngines: ["google-b-m", "bing", "baidu", "ddg", "wikipedia-zh-CN"]
        // searchOrder (default): ["Google", "Bing"]

        assertEquals(
            listOf("google-b-m", "bing", "baidu", "ddg", "wikipedia-zh-CN"),
            searchEngines.map { it.id },
        )

        // searchDefault: "百度"
        val default = searchEngines.find { it.id == engines.defaultSearchEngineId }
        assertNotNull(default)
        assertEquals("baidu", default!!.id)
    }

    @Test
    fun `Load search engines for ru_RU locale with engines not in searchOrder`() = runTest {
        val storage = BundledSearchEnginesStorage(testContext)
        val engines = storage.load(RegionState("RU", "RU"), Locale("ru", "RU"))
        val searchEngines = engines.list

        assertEquals(
            listOf("google-com-nocodes", "ddg", "wikipedia-ru"),
            searchEngines.map { it.id },
        )

        // searchDefault: "Google"
        val default = searchEngines.find { it.id == engines.defaultSearchEngineId }
        assertNotNull(default)
        assertEquals("google-com-nocodes", default!!.id)
    }

    @Test
    fun `Load search engines for trs locale with non-google initial engines and no default`() = runTest {
        val storage = BundledSearchEnginesStorage(testContext)
        val engines = storage.load(RegionState.Default, Locale("trs", ""))
        val searchEngines = engines.list

        // visibleDefaultEngines: ["google-b-m", "bing", "amazondotcom", "ddg", "wikipedia-es"]
        // searchOrder (default): ["Google", "Bing"]

        assertEquals(
            listOf("google-b-m", "bing", "amazondotcom", "ddg", "wikipedia-es"),
            searchEngines.map { it.id },
        )

        // searchDefault (default): "Google"
        val default = searchEngines.find { it.id == engines.defaultSearchEngineId }
        assertNotNull(default)
        assertEquals("google-b-m", default!!.id)
    }

    @Test
    fun `Load search engines for locale not in configuration`() = runTest {
        val storage = BundledSearchEnginesStorage(testContext)
        val engines = storage.load(RegionState.Default, Locale("xx", "XX"))
        val searchEngines = engines.list

        assertEquals(5, searchEngines.size)
    }

    private fun assertContainsSearchEngine(identifier: String, searchEngines: List<SearchEngine>) {
        searchEngines.forEach {
            if (identifier == it.id) {
                return
            }
        }
        throw AssertionError("Search engine $identifier not in list")
    }

    private fun assertContainsNotSearchEngine(identifier: String, searchEngines: List<SearchEngine>) {
        searchEngines.forEach {
            if (identifier == it.id) {
                throw AssertionError("Search engine $identifier in list")
            }
        }
    }

    @Test
    fun `Verify values of Google search engine`() = runTest {
        val storage = BundledSearchEnginesStorage(testContext)

        val engines = storage.load(RegionState("US", "US"), Locale("en", "US"))
        val searchEngines = engines.list

        assertEquals(6, searchEngines.size)

        val google = searchEngines.find { it.name == "Google" }
        assertNotNull(google!!)

        assertEquals("google-b-1-m", google.id)
        assertEquals("Google", google.name)
        assertEquals(SearchEngine.Type.BUNDLED, google.type)
        assertNotNull(google.icon)
        assertEquals("https://www.google.com/complete/search?client=firefox&q={searchTerms}", google.suggestUrl)
        assertTrue(google.resultUrls.isNotEmpty())
    }
}
