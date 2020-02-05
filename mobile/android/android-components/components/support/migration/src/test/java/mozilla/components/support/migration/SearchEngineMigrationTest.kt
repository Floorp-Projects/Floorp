/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.browser.search.provider.AssetsSearchEngineProvider
import mozilla.components.browser.search.provider.localization.SearchLocalization
import mozilla.components.browser.search.provider.localization.SearchLocalizationProvider
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class SearchEngineMigrationTest {
    private val context
        get() = ApplicationProvider.getApplicationContext<Context>()

    @Test
    fun `default Google with en_US_US list`() {
        val (searchEngineManager, result) = migrate(
            fennecDefault = "Google",
            language = "en",
            country = "US",
            region = "US"
        )

        assertEquals("Google", searchEngineManager.defaultSearchEngine?.name)
        assertEquals("Google", searchEngineManager.getDefaultSearchEngine(context).name)
        assertIsSuccess(
            SearchEngineMigrationResult.Success.SearchEngineMigrated,
            result
        )
    }

    @Test
    fun `default DuckDuckGo with en_US_US list`() {
        val (searchEngineManager, result) = migrate(
            fennecDefault = "DuckDuckGo",
            language = "en",
            country = "US",
            region = "US"
        )

        assertEquals("DuckDuckGo", searchEngineManager.defaultSearchEngine?.name)
        assertEquals("DuckDuckGo", searchEngineManager.getDefaultSearchEngine(context).name)
        assertIsSuccess(
            SearchEngineMigrationResult.Success.SearchEngineMigrated,
            result
        )
    }

    @Test
    fun `default Bing with de_DE_DE list`() {
        val (searchEngineManager, result) = migrate(
            fennecDefault = "Bing",
            language = "de",
            country = "DE",
            region = "DE"
        )

        assertEquals("Bing", searchEngineManager.defaultSearchEngine?.name)
        assertEquals("Bing", searchEngineManager.getDefaultSearchEngine(context).name)
        assertIsSuccess(
            SearchEngineMigrationResult.Success.SearchEngineMigrated,
            result
        )
    }

    @Test
    fun `default Qwant with de_DE_DE list`() {
        val (searchEngineManager, result) = migrate(
            fennecDefault = "Qwant",
            language = "de",
            country = "DE",
            region = "DE"
        )

        assertEquals("Qwant", searchEngineManager.defaultSearchEngine?.name)
        assertEquals("Qwant", searchEngineManager.getDefaultSearchEngine(context).name)
        assertIsSuccess(
            SearchEngineMigrationResult.Success.SearchEngineMigrated,
            result
        )
    }

    @Test
    fun `default Qwant with en_US_US list`() {
        val (searchEngineManager, result) = migrate(
            fennecDefault = "Qwant",
            language = "en",
            country = "US",
            region = "US"
        )

        // Quant is not in the en_US_US list and therefore no default was set
        assertNull(searchEngineManager.defaultSearchEngine?.name)
        assertEquals("Google", searchEngineManager.getDefaultSearchEngine(context).name)
        assertIsFailure(
            SearchEngineMigrationResult.Failure.NoMatch,
            result
        )
    }

    @Test
    fun `default null with en_US_US list`() {
        val (searchEngineManager, result) = migrate(
            fennecDefault = null,
            language = "en",
            country = "US",
            region = "US"
        )

        assertEquals(null, searchEngineManager.defaultSearchEngine?.name)
        assertEquals("Google", searchEngineManager.getDefaultSearchEngine(context).name)
        assertIsFailure(
            SearchEngineMigrationResult.Failure.NoDefault,
            result
        )
    }
}

private fun assertIsSuccess(
    expected: SearchEngineMigrationResult.Success,
    value: Result<SearchEngineMigrationResult>
) {
    assertTrue(value is Result.Success<SearchEngineMigrationResult>)

    val result = (value as Result.Success<SearchEngineMigrationResult>).value
    assertEquals(expected, result)
}

private fun assertIsFailure(
    expected: SearchEngineMigrationResult.Failure,
    value: Result<SearchEngineMigrationResult>
) {
    assertTrue(value is Result.Failure<SearchEngineMigrationResult>)

    val result = (value as Result.Failure<SearchEngineMigrationResult>)
    val wrapper = result.throwables.first() as SearchEngineMigrationException

    assertEquals(expected, wrapper.failure)
}

private fun migrate(
    fennecDefault: String?,
    language: String,
    country: String,
    region: String
): Pair<SearchEngineManager, Result<SearchEngineMigrationResult>> {
    return runBlocking {
        val searchEngineManager = searchEngineManagerFor(
            language,
            country,
            region
        )

        if (fennecDefault != null) {
            ApplicationProvider.getApplicationContext<Context>().getSharedPreferences(
                FennecSettingsMigration.FENNEC_APP_SHARED_PREFS_NAME,
                Context.MODE_PRIVATE
            ).edit()
                .putString("search.engines.defaultname", fennecDefault)
                .apply()
        }

        val result = SearchEngineMigration.migrate(
            ApplicationProvider.getApplicationContext(),
            searchEngineManager
        )

        Pair(searchEngineManager, result)
    }
}

private fun searchEngineManagerFor(language: String, country: String, region: String): SearchEngineManager {
    val localizationProvider = object : SearchLocalizationProvider {
        override suspend fun determineRegion() = SearchLocalization(language, country, region)
    }

    val searchEngineProvider = AssetsSearchEngineProvider(localizationProvider)

    return SearchEngineManager(
        providers = listOf(
            searchEngineProvider
        )
    )
}
