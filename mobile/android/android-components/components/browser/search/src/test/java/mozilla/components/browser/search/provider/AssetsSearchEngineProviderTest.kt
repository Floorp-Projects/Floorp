/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.search.provider

import android.content.Context
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.search.provider.filter.SearchEngineFilter
import mozilla.components.browser.search.provider.localization.SearchLocalization
import mozilla.components.browser.search.provider.localization.SearchLocalizationProvider
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class AssetsSearchEngineProviderTest {

    @Test
    fun `Load search engines for en-US from assets`() = runBlocking {
        val localizationProvider = object : SearchLocalizationProvider {
            override suspend fun determineRegion() = SearchLocalization("en", "US", "US")
        }

        val searchEngineProvider = AssetsSearchEngineProvider(localizationProvider)

        val engines = searchEngineProvider.loadSearchEngines(testContext)
        val searchEngines = engines.list

        assertEquals(6, searchEngines.size)
    }

    @Test
    fun `Load search engines for en-US with with filter`() = runBlocking {
        val localizationProvider = object : SearchLocalizationProvider {
            override suspend fun determineRegion() = SearchLocalization("en", "US", "US")
        }

        val filter = object : SearchEngineFilter {
            private val exclude = listOf("yahoo", "bing", "ddg")

            override fun filter(context: Context, searchEngine: SearchEngine): Boolean {
                return !exclude.contains(searchEngine.identifier)
            }
        }

        val searchEngineProvider = AssetsSearchEngineProvider(localizationProvider, listOf(filter))

        val engines = searchEngineProvider.loadSearchEngines(testContext)
        val searchEngines = engines.list

        assertEquals(4, searchEngines.size)
    }

    @Test
    fun `Load search engines for de-DE with global US region override`() = runBlocking {
        // Without region
        run {
            val localizationProviderWithoutRegion = object : SearchLocalizationProvider {
                override suspend fun determineRegion() = SearchLocalization("de", "DE")
            }

            val searchEngineProvider = AssetsSearchEngineProvider(localizationProviderWithoutRegion)
            val engines = searchEngineProvider.loadSearchEngines(testContext)
            val searchEngines = engines.list

            assertEquals(7, searchEngines.size)
            assertContainsSearchEngine("google-b-m", searchEngines)
            assertContainsNotSearchEngine("google-2018", searchEngines)
        }
        // With region
        run {
            val localizationProviderWithRegion = object : SearchLocalizationProvider {
                override suspend fun determineRegion() = SearchLocalization("de", "DE", "US")
            }

            val searchEngineProvider = AssetsSearchEngineProvider(localizationProviderWithRegion)
            val engines = searchEngineProvider.loadSearchEngines(testContext)
            val searchEngines = engines.list

            assertEquals(7, searchEngines.size)
            assertContainsSearchEngine("google-b-1-m", searchEngines)
            assertContainsNotSearchEngine("google", searchEngines)
        }
    }

    @Test
    fun `Load search engines for en-US with local RU region override`() = runBlocking {
        // Without region
        run {
            val localizationProviderWithoutRegion = object : SearchLocalizationProvider {
                override suspend fun determineRegion() = SearchLocalization("en", "US")
            }

            val searchEngineProvider = AssetsSearchEngineProvider(localizationProviderWithoutRegion)
            val engines = searchEngineProvider.loadSearchEngines(testContext)
            val searchEngines = engines.list

            println("searchEngines = $searchEngines")
            assertEquals(6, searchEngines.size)
            assertContainsNotSearchEngine("yandex-en", searchEngines)
        }
        // With region
        run {
            val localizationProviderWithRegion = object : SearchLocalizationProvider {
                override suspend fun determineRegion() = SearchLocalization("en", "US", "RU")
            }

            val searchEngineProvider = AssetsSearchEngineProvider(localizationProviderWithRegion)
            val engines = searchEngineProvider.loadSearchEngines(testContext)
            val searchEngines = engines.list

            println("searchEngines = $searchEngines")
            assertEquals(7, searchEngines.size)
            assertContainsSearchEngine("yandex-en", searchEngines)
        }
    }

    @Test
    fun `Load search engines for zh-CN_CN locale with searchDefault override`() = runBlocking {
        val provider = object : SearchLocalizationProvider {
            override suspend fun determineRegion() = SearchLocalization("zh", "CN", "CN")
        }

        val searchEngineProvider = AssetsSearchEngineProvider(provider)
        val engines = searchEngineProvider.loadSearchEngines(testContext)
        val searchEngines = engines.list

        // visibleDefaultEngines: ["google-b-m", "baidu", "bing", "taobao", "wikipedia-zh-CN"]
        // searchOrder (default): ["Google", "Bing"]

        assertEquals(
            listOf("google-b-m", "bing", "baidu", "taobao", "wikipedia-zh-CN"),
            searchEngines.map { it.identifier }
        )

        // searchDefault: "百度"
        assertEquals("baidu", engines.default?.identifier)
    }

    @Test
    fun `Load search engines for ru_RU locale with engines not in searchOrder`() = runBlocking {
        val provider = object : SearchLocalizationProvider {
            override suspend fun determineRegion() = SearchLocalization("ru", "RU", "RU")
        }

        val searchEngineProvider = AssetsSearchEngineProvider(provider)
        val engines = searchEngineProvider.loadSearchEngines(testContext)
        val searchEngines = engines.list

        // visibleDefaultEngines: ["google-b-m", "yandex-ru", "twitter", "wikipedia-ru"]
        // searchOrder (default): ["Google", "Bing"]

        assertEquals(
            listOf("google-b-m", "yandex-ru", "twitter", "wikipedia-ru"),
            searchEngines.map { it.identifier }
        )

        // searchDefault: "Яндекс"
        assertEquals("yandex-ru", engines.default?.identifier)
    }

    @Test
    fun `Load search engines for trs locale with non-google initial engines and no default`() = runBlocking {
        val provider = object : SearchLocalizationProvider {
            override suspend fun determineRegion() = SearchLocalization("trs", "", "")
        }

        val searchEngineProvider = AssetsSearchEngineProvider(provider)
        val engines = searchEngineProvider.loadSearchEngines(testContext)
        val searchEngines = engines.list

        // visibleDefaultEngines: ["amazondotcom", "bing", "google-b-m", "twitter", "wikipedia-es"]
        // searchOrder (default): ["Google", "Bing"]

        assertEquals(
            listOf("google-b-m", "bing", "amazondotcom", "twitter", "wikipedia-es"),
            searchEngines.map { it.identifier }
        )

        // searchDefault (default): "Google"
        assertEquals("google-b-m", engines.default?.identifier)
    }

    @Test
    fun `load search engines for locale not in configuration`() = runBlocking {
        val provider = object : SearchLocalizationProvider {
            override suspend fun determineRegion() = SearchLocalization("xx", "XX", "XX")
        }

        val searchEngineProvider = AssetsSearchEngineProvider(provider)
        val engines = searchEngineProvider.loadSearchEngines(testContext)
        val searchEngines = engines.list

        assertEquals(6, searchEngines.size)
    }

    @Test
    fun `provider loads additional identifiers`() {
        val usProvider = object : SearchLocalizationProvider {
            override suspend fun determineRegion() = SearchLocalization("en", "US", "US")
        }

        // Loading "en-US" without additional identifiers
        runBlocking {
            val searchEngineProvider = AssetsSearchEngineProvider(usProvider)
            val engines = searchEngineProvider.loadSearchEngines(testContext)
            val searchEngines = engines.list

            assertEquals(6, searchEngines.size)
            assertContainsNotSearchEngine("duckduckgo", searchEngines)
        }

        // Load "en-US" with "duckduckgo" added
        runBlocking {
            val provider = AssetsSearchEngineProvider(
                    usProvider,
                    additionalIdentifiers = listOf("duckduckgo"))
            val engines = provider.loadSearchEngines(testContext)
            val searchEngines = engines.list

            assertEquals(7, searchEngines.size)
            assertContainsSearchEngine("duckduckgo", searchEngines)
        }
    }

    @Test
    fun `provider loads additional identifiers if search order specified`() {
        val usProvider = object : SearchLocalizationProvider {
            override suspend fun determineRegion() = SearchLocalization("en", "US", "US")
        }

        // Loading "en-US" without additional identifiers. This will
        // contain google-b-m, but not google.
        runBlocking {
            val searchEngineProvider = AssetsSearchEngineProvider(usProvider)
            val engines = searchEngineProvider.loadSearchEngines(testContext)
            val searchEngines = engines.list

            assertEquals(6, searchEngines.size)
            assertContainsNotSearchEngine("google", searchEngines)
        }

        // Load "en-US" with "google" added. Apps may add custom search plugins
        // and reuse existing plugin names to make their own product-specific
        // decision on which engine to use. So we need to return them all by
        // default i.e. the presence of a search order should not prevent us
        // from returning the custom engines.
        // See: https://github.com/mozilla-mobile/firefox-tv/pull/1876/
        runBlocking {
            val provider = AssetsSearchEngineProvider(
                    usProvider,
                    additionalIdentifiers = listOf("google"))
            val engines = provider.loadSearchEngines(testContext)
            val searchEngines = engines.list

            assertEquals(7, searchEngines.size)
            assertContainsSearchEngine("google", searchEngines)
        }
    }

    private fun assertContainsSearchEngine(identifier: String, searchEngines: List<SearchEngine>) {
        searchEngines.forEach {
            if (identifier == it.identifier) {
                return
            }
        }
        throw AssertionError("Search engine $identifier not in list")
    }

    private fun assertContainsNotSearchEngine(identifier: String, searchEngines: List<SearchEngine>) {
        searchEngines.forEach {
            if (identifier == it.identifier) {
                throw AssertionError("Search engine $identifier in list")
            }
        }
    }
}
