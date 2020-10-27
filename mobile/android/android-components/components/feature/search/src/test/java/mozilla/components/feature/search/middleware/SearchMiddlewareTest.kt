/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.middleware

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.TestCoroutineDispatcher
import mozilla.components.browser.state.action.SearchAction
import mozilla.components.browser.state.search.RegionState
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.search.storage.CustomSearchEngineStorage
import mozilla.components.feature.search.storage.SearchMetadataStorage
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions
import java.util.Locale
import java.util.UUID

@RunWith(AndroidJUnit4::class)
class SearchMiddlewareTest {
    private lateinit var dispatcher: TestCoroutineDispatcher
    private lateinit var originalLocale: Locale
    private lateinit var scope: CoroutineScope

    @Before
    fun setUp() {
        dispatcher = TestCoroutineDispatcher()
        scope = CoroutineScope(dispatcher)
        originalLocale = Locale.getDefault()
    }

    @After
    fun tearDown() {
        dispatcher.cleanupTestCoroutines()
        scope.cancel()

        if (Locale.getDefault() != originalLocale) {
            Locale.setDefault(originalLocale)
        }
    }

    @Test
    fun `Loads search engines for region`() {
        val searchMiddleware = SearchMiddleware(
            testContext,
            ioDispatcher = dispatcher,
            customStorage = CustomSearchEngineStorage(testContext, dispatcher)
        )

        val store = BrowserStore(
            middleware = listOf(searchMiddleware)
        )

        assertTrue(store.state.search.regionSearchEngines.isEmpty())

        store.dispatch(SearchAction.SetRegionAction(
            RegionState("US", "US")
        )).joinBlocking()

        wait(store, dispatcher)

        assertTrue(store.state.search.regionSearchEngines.isNotEmpty())
    }

    @Test
    fun `Loads custom search engines`() {
        val searchEngine = SearchEngine(
            id = "test-search",
            name = "Test Engine",
            icon = mock(),
            type = SearchEngine.Type.CUSTOM,
            resultUrls = listOf(),
            suggestUrl = null
        )

        val storage = CustomSearchEngineStorage(testContext, dispatcher)
        runBlocking { storage.saveSearchEngine(searchEngine) }

        val store = BrowserStore(
            middleware = listOf(
                SearchMiddleware(
                    testContext,
                    ioDispatcher = dispatcher,
                    customStorage = storage
                )
            )
        )

        store.dispatch(
            SearchAction.SetRegionAction(RegionState.Default)
        ).joinBlocking()

        wait(store, dispatcher)

        assertTrue(store.state.search.customSearchEngines.isNotEmpty())
        assertNull(store.state.search.userSelectedSearchEngineId)
    }

    @Test
    fun `Loads default search engine ID`() {
        val storage = SearchMetadataStorage(testContext)
        runBlocking { storage.setUserSelectedSearchEngineId("test-id") }

        val middleware = SearchMiddleware(
            testContext,
            ioDispatcher = dispatcher,
            metadataStorage = storage,
            customStorage = CustomSearchEngineStorage(testContext, dispatcher)
        )

        val store = BrowserStore(
            middleware = listOf(middleware)
        )

        store.dispatch(
            SearchAction.SetRegionAction(RegionState.Default)
        ).joinBlocking()

        wait(store, dispatcher)

        assertEquals("test-id", store.state.search.userSelectedSearchEngineId)
    }

    @Test
    fun `Update default search engine`() {
        val storage = SearchMetadataStorage(testContext)
        val id = "test-id-${UUID.randomUUID()}"

        run {
            val store = BrowserStore(middleware = listOf(SearchMiddleware(
                testContext,
                ioDispatcher = dispatcher,
                metadataStorage = storage,
                customStorage = CustomSearchEngineStorage(testContext, dispatcher)
            )))

            store.dispatch(
                SearchAction.SetRegionAction(RegionState.Default)
            ).joinBlocking()

            wait(store, dispatcher)

            assertNull(store.state.search.userSelectedSearchEngineId)

            store.dispatch(SearchAction.SelectSearchEngineAction(id)).joinBlocking()

            wait(store, dispatcher)

            assertEquals(id, store.state.search.userSelectedSearchEngineId)
        }

        run {
            val store = BrowserStore(middleware = listOf(SearchMiddleware(
                testContext,
                ioDispatcher = dispatcher,
                metadataStorage = storage,
                customStorage = CustomSearchEngineStorage(testContext, dispatcher)
            )))

            store.dispatch(
                SearchAction.SetRegionAction(RegionState.Default)
            ).joinBlocking()

            wait(store, dispatcher)

            assertEquals(id, store.state.search.userSelectedSearchEngineId)
        }
    }

    @Test
    fun `Custom search engines - Create, Update, Delete`() {
        runBlocking {
            val storage: SearchMiddleware.CustomStorage = mock()
            doReturn(emptyList<SearchEngine>()).`when`(storage).loadSearchEngineList()

            val store = BrowserStore(middleware = listOf(
                SearchMiddleware(
                    testContext,
                    ioDispatcher = dispatcher,
                    customStorage = storage
                )
            ))

            store.dispatch(
                SearchAction.SetRegionAction(RegionState.Default)
            ).joinBlocking()

            wait(store, dispatcher)

            assertTrue(store.state.search.customSearchEngines.isEmpty())
            verify(storage).loadSearchEngineList()
            verifyNoMoreInteractions(storage)

            // Add a custom search engine

            val engine1 = SearchEngine("test-id-1", "test engine one", mock(), type = SearchEngine.Type.CUSTOM)
            store.dispatch(SearchAction.UpdateCustomSearchEngineAction(engine1)).joinBlocking()

            wait(store, dispatcher)

            assertTrue(store.state.search.customSearchEngines.isNotEmpty())
            assertEquals(1, store.state.search.customSearchEngines.size)
            verify(storage).saveSearchEngine(engine1)
            verifyNoMoreInteractions(storage)

            // Add another custom search engine

            val engine2 = SearchEngine("test-id-2", "test engine two", mock(), type = SearchEngine.Type.CUSTOM)
            store.dispatch(SearchAction.UpdateCustomSearchEngineAction(engine2)).joinBlocking()

            wait(store, dispatcher)

            assertTrue(store.state.search.customSearchEngines.isNotEmpty())
            assertEquals(2, store.state.search.customSearchEngines.size)
            verify(storage).saveSearchEngine(engine2)
            verifyNoMoreInteractions(storage)

            assertEquals("test engine one", store.state.search.customSearchEngines[0].name)
            assertEquals("test engine two", store.state.search.customSearchEngines[1].name)

            // Update first engine

            val updated = engine1.copy(
                name = "updated engine"
            )
            store.dispatch(SearchAction.UpdateCustomSearchEngineAction(updated)).joinBlocking()

            wait(store, dispatcher)

            assertTrue(store.state.search.customSearchEngines.isNotEmpty())
            assertEquals(2, store.state.search.customSearchEngines.size)
            verify(storage).saveSearchEngine(updated)
            verifyNoMoreInteractions(storage)

            assertEquals("updated engine", store.state.search.customSearchEngines[0].name)
            assertEquals("test engine two", store.state.search.customSearchEngines[1].name)

            // Remove second engine

            store.dispatch(SearchAction.RemoveCustomSearchEngineAction(engine2.id)).joinBlocking()

            wait(store, dispatcher)

            assertTrue(store.state.search.customSearchEngines.isNotEmpty())
            assertEquals(1, store.state.search.customSearchEngines.size)
            verify(storage).removeSearchEngine(engine2.id)
            verifyNoMoreInteractions(storage)

            assertEquals("updated engine", store.state.search.customSearchEngines[0].name)
        }
    }

    @Test
    fun `Hiding and showing search engines`() {
        val searchMiddleware = SearchMiddleware(
            testContext,
            ioDispatcher = dispatcher,
            customStorage = CustomSearchEngineStorage(testContext, dispatcher),
            metadataStorage = SearchMetadataStorage(testContext)
        )

        val google = BrowserStore(middleware = listOf(searchMiddleware)).let { store ->
            store.dispatch(SearchAction.SetRegionAction(
                RegionState("US", "US")
            )).joinBlocking()

            wait(store, dispatcher)

            store.state.search.regionSearchEngines.find { searchEngine -> searchEngine.name == "Google" }
        }
        assertNotNull(google!!)

        run {
            val store = BrowserStore(middleware = listOf(searchMiddleware))

            store.dispatch(SearchAction.SetRegionAction(
                RegionState("US", "US")
            )).joinBlocking()

            wait(store, dispatcher)

            assertNotNull(store.state.search.regionSearchEngines.find { it.id == google.id })
            assertEquals(0, store.state.search.hiddenSearchEngines.size)

            store.dispatch(
                SearchAction.HideSearchEngineAction(google.id)
            ).joinBlocking()

            wait(store, dispatcher)

            assertNull(store.state.search.regionSearchEngines.find { it.id == google.id })
            assertEquals(1, store.state.search.hiddenSearchEngines.size)
            assertNotNull(store.state.search.hiddenSearchEngines.find { it.id == google.id })
        }

        run {
            val store = BrowserStore(middleware = listOf(searchMiddleware))

            store.dispatch(SearchAction.SetRegionAction(
                RegionState("US", "US")
            )).joinBlocking()

            wait(store, dispatcher)

            assertNull(store.state.search.regionSearchEngines.find { it.id == google.id })
            assertEquals(1, store.state.search.hiddenSearchEngines.size)
            assertNotNull(store.state.search.hiddenSearchEngines.find { it.id == google.id })

            store.dispatch(
                SearchAction.ShowSearchEngineAction(google.id)
            ).joinBlocking()

            assertNotNull(store.state.search.regionSearchEngines.find { it.id == google.id })
            assertEquals(0, store.state.search.hiddenSearchEngines.size)
        }

        run {
            val store = BrowserStore(middleware = listOf(searchMiddleware))

            store.dispatch(SearchAction.SetRegionAction(
                RegionState("US", "US")
            )).joinBlocking()

            wait(store, dispatcher)

            assertNotNull(store.state.search.regionSearchEngines.find { it.id == google.id })
            assertEquals(0, store.state.search.hiddenSearchEngines.size)
        }
    }
}

private fun wait(store: BrowserStore, dispatcher: TestCoroutineDispatcher) {
    // First we wait for the InitAction that may still need to be processed.
    store.waitUntilIdle()

    // Now we wait for the Middleware that may need to asynchronously process an action the test dispatched
    dispatcher.advanceUntilIdle()

    // Since the Middleware may have dispatched an action, we now wait for the store again.
    store.waitUntilIdle()
}