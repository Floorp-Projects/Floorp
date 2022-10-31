/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.ext

import mozilla.components.browser.state.action.SearchAction
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SearchState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

class BrowserStoreKtTest {
    @Test
    fun `waitForDefaultSearchEngine - with state already loaded`() {
        val store = BrowserStore(
            BrowserState(
                search = SearchState(
                    regionSearchEngines = listOf(
                        SearchEngine(
                            id = "google",
                            name = "Google",
                            icon = mock(),
                            type = SearchEngine.Type.BUNDLED,
                        ),
                    ),
                    userSelectedSearchEngineId = "google",
                    complete = true,
                ),
            ),
        )

        val latch = CountDownLatch(1)

        store.waitForSelectedOrDefaultSearchEngine { searchEngine ->
            assertNotNull(searchEngine)
            assertEquals("google", searchEngine!!.id)
            latch.countDown()
        }

        assertTrue(latch.await(10, TimeUnit.SECONDS))
    }

    @Test
    fun `waitForDefaultSearchEngine - with state dispatched later`() {
        val store = BrowserStore()

        val latch = CountDownLatch(1)

        store.waitForSelectedOrDefaultSearchEngine { searchEngine ->
            assertNotNull(searchEngine)
            assertEquals("google", searchEngine!!.id)
            latch.countDown()
        }

        store.dispatch(
            SearchAction.SetSearchEnginesAction(
                regionSearchEngines = listOf(
                    SearchEngine(
                        id = "google",
                        name = "Google",
                        icon = mock(),
                        type = SearchEngine.Type.BUNDLED,
                    ),
                ),
                userSelectedSearchEngineId = null,
                userSelectedSearchEngineName = null,
                regionDefaultSearchEngineId = "google",
                customSearchEngines = emptyList(),
                hiddenSearchEngines = emptyList(),
                additionalAvailableSearchEngines = emptyList(),
                additionalSearchEngines = emptyList(),
                regionSearchEnginesOrder = listOf("google"),
            ),
        )

        assertTrue(latch.await(10, TimeUnit.SECONDS))
    }

    @Test
    fun `waitForDefaultSearchEngine - no default was loaded`() {
        val store = BrowserStore()

        val latch = CountDownLatch(1)

        store.waitForSelectedOrDefaultSearchEngine { searchEngine ->
            assertNull(searchEngine)
            latch.countDown()
        }

        store.dispatch(
            SearchAction.SetSearchEnginesAction(
                regionSearchEngines = listOf(),
                userSelectedSearchEngineId = null,
                userSelectedSearchEngineName = null,
                regionDefaultSearchEngineId = "default",
                customSearchEngines = emptyList(),
                hiddenSearchEngines = emptyList(),
                additionalAvailableSearchEngines = emptyList(),
                additionalSearchEngines = emptyList(),
                regionSearchEnginesOrder = listOf("google"),
            ),
        )

        assertTrue(latch.await(10, TimeUnit.SECONDS))
    }
}
