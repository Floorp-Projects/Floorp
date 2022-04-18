/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.UnconfinedTestDispatcher
import mozilla.components.browser.state.action.SearchAction
import mozilla.components.browser.state.search.RegionState
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.search.middleware.SearchMiddleware
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import java.util.Locale

@RunWith(AndroidJUnit4::class)
class SearchEngineMigrationTest {
    private lateinit var originalLocale: Locale

    @Before
    fun setUp() {
        originalLocale = Locale.getDefault()
    }

    @After
    fun tearDown() {
        if (Locale.getDefault() != originalLocale) {
            Locale.setDefault(originalLocale)
        }
    }

    @Test
    fun `default Google with en_US_US list`() {
        val (store, result) = migrate(
            fennecDefault = "Google",
            language = "en",
            country = "US",
            region = "US"
        )

        val defaultEngine = store.state.search.selectedOrDefaultSearchEngine
        assertNotNull(defaultEngine)
        assertEquals("Google", defaultEngine?.name)
        assertIsSuccess(
            SearchEngineMigrationResult.Success.SearchEngineMigrated,
            result
        )
    }

    @Test
    fun `default DuckDuckGo with en_US_US list`() {
        val (store, result) = migrate(
            fennecDefault = "DuckDuckGo",
            language = "en",
            country = "US",
            region = "US"
        )

        val defaultEngine = store.state.search.selectedOrDefaultSearchEngine
        assertNotNull(defaultEngine)
        assertEquals("DuckDuckGo", defaultEngine?.name)
        assertIsSuccess(
            SearchEngineMigrationResult.Success.SearchEngineMigrated,
            result
        )
    }

    @Test
    fun `default Bing with de_DE_DE list`() {
        val (store, result) = migrate(
            fennecDefault = "Bing",
            language = "de",
            country = "DE",
            region = "DE"
        )

        val defaultEngine = store.state.search.selectedOrDefaultSearchEngine
        assertNotNull(defaultEngine)
        assertEquals("Bing", defaultEngine?.name)
        assertIsSuccess(
            SearchEngineMigrationResult.Success.SearchEngineMigrated,
            result
        )
    }

    @Test
    fun `default Qwant with de_DE_DE list`() {
        val (store, result) = migrate(
            fennecDefault = "Qwant",
            language = "de",
            country = "DE",
            region = "DE"
        )

        val defaultEngine = store.state.search.selectedOrDefaultSearchEngine
        assertNotNull(defaultEngine)
        assertEquals("Qwant", defaultEngine?.name)
        assertIsSuccess(
            SearchEngineMigrationResult.Success.SearchEngineMigrated,
            result
        )
    }

    @Test
    fun `default Qwant with en_US_US list`() {
        val (store, result) = migrate(
            fennecDefault = "Qwant",
            language = "en",
            country = "US",
            region = "US"
        )

        // Qwant is not in the en_US_US list and therefore no default was set
        val defaultEngine = store.state.search.selectedOrDefaultSearchEngine
        assertNotNull(defaultEngine)
        assertEquals("Google", defaultEngine?.name)
        assertIsFailure(
            SearchEngineMigrationResult.Failure.NoMatch,
            result
        )
    }

    @Test
    fun `default null with en_US_US list`() {
        val (store, result) = migrate(
            fennecDefault = null,
            language = "en",
            country = "US",
            region = "US"
        )

        val defaultEngine = store.state.search.selectedOrDefaultSearchEngine
        assertNotNull(defaultEngine)
        assertEquals("Google", defaultEngine?.name)
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
): Pair<BrowserStore, Result<SearchEngineMigrationResult>> {
    return runBlocking {
        val store = storeFor(
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
            store
        )

        store.waitUntilIdle()

        Pair(store, result)
    }
}

private fun storeFor(language: String, country: String, region: String): BrowserStore {
    val dispatcher = UnconfinedTestDispatcher()

    Locale.setDefault(Locale(language, country))

    val store = BrowserStore(
        middleware = listOf(
            SearchMiddleware(testContext, ioDispatcher = dispatcher)
        )
    )

    store.dispatch(
        SearchAction.SetRegionAction(
            RegionState(region, region)
        )
    ).joinBlocking()

    // First we wait for the InitAction that may still need to be processed.
    store.waitUntilIdle()

    // Now we wait for the Middleware that may need to asynchronously process an action the test dispatched
    dispatcher.scheduler.advanceUntilIdle()

    // Since the Middleware may have dispatched an action, we now wait for the store again.
    store.waitUntilIdle()

    return store
}
