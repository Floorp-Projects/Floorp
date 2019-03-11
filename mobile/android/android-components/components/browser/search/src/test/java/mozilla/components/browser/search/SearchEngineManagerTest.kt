/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.search

import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.net.Uri
import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.search.provider.SearchEngineProvider
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.shadows.ShadowApplication
import java.util.UUID

@RunWith(RobolectricTestRunner::class)
class SearchEngineManagerTest {
    @Test
    fun `manager returns engines from provider`() {
        runBlocking {
            val provider = mockProvider(listOf(
                    mockSearchEngine("mozsearch"),
                    mockSearchEngine("google"),
                    mockSearchEngine("bing")))

            val manager = SearchEngineManager(listOf(provider))

            manager.load(RuntimeEnvironment.application)

            val engines = manager.getSearchEngines(RuntimeEnvironment.application)
            assertEquals(3, engines.size)

            engines.assertContainsIdentifier("mozsearch")
            engines.assertContainsIdentifier("google")
            engines.assertContainsIdentifier("bing")
        }
    }

    @Test
    fun `manager will load search engines on first get if not loaded previously`() {
        runBlocking {
            val provider = mockProvider(listOf(
                    mockSearchEngine("mozsearch"),
                    mockSearchEngine("google"),
                    mockSearchEngine("bing")))

            val manager = SearchEngineManager(listOf(provider))

            val engines = manager.getSearchEngines(RuntimeEnvironment.application)
            assertEquals(3, engines.size)
        }
    }

    @Test
    fun `manager returns first engine if default cannot be found`() {
        runBlocking {
            val provider = mockProvider(listOf(
                    mockSearchEngine("mozsearch"),
                    mockSearchEngine("google"),
                    mockSearchEngine("bing")))

            val manager = SearchEngineManager(listOf(provider))

            val default = manager.getDefaultSearchEngine(RuntimeEnvironment.application, "banana")
            assertEquals("mozsearch", default.identifier)
        }
    }

    @Test
    fun `manager returns default engine with identifier if it exists`() {
        runBlocking {
            val provider = mockProvider(listOf(
                    mockSearchEngine("mozsearch", "Mozilla Search"),
                    mockSearchEngine("google", "Google Search"),
                    mockSearchEngine("bing", "Bing Search")))

            val manager = SearchEngineManager(listOf(provider))

            val default = manager.getDefaultSearchEngine(
                    RuntimeEnvironment.application,
                    "Bing Search")

            assertEquals("bing", default.identifier)
        }
    }

    @Test
    fun `manager returns first engine as default if no identifier is specified`() {
        runBlocking {
            val provider = mockProvider(listOf(
                    mockSearchEngine("mozsearch"),
                    mockSearchEngine("google"),
                    mockSearchEngine("bing")))

            val manager = SearchEngineManager(listOf(provider))

            val default = manager.getDefaultSearchEngine(RuntimeEnvironment.application)
            assertEquals("mozsearch", default.identifier)
        }
    }

    @Test
    fun `manager returns set default engine as default when no identifier is specified`() {
        runBlocking {
            val provider = mockProvider(
                listOf(
                    mockSearchEngine("mozsearch"),
                    mockSearchEngine("google"),
                    mockSearchEngine("bing")
                )
            )

            val manager = SearchEngineManager(listOf(provider))
            manager.defaultSearchEngine = mockSearchEngine("bing")

            val default = manager.getDefaultSearchEngine(RuntimeEnvironment.application)
            assertEquals("bing", default.identifier)
        }
    }

    @Test
    fun `manager returns engine from identifier as default when identifier is specified`() {
        runBlocking {
            val provider = mockProvider(
                listOf(
                    mockSearchEngine("mozsearch", "Mozilla Search"),
                    mockSearchEngine("google", "Google Search"),
                    mockSearchEngine("bing", "Bing Search")
                )
            )

            val manager = SearchEngineManager(listOf(provider))
            manager.defaultSearchEngine = mockSearchEngine("bing")

            val default =
                manager.getDefaultSearchEngine(RuntimeEnvironment.application, "Google Search")
            assertEquals("google", default.identifier)
        }
    }

    @Ignore
    @Test
    fun `locale update broadcast will trigger reload`() {
        runBlocking {
            val provider = spy(mockProvider(listOf(
                    mockSearchEngine("mozsearch"),
                    mockSearchEngine("google"),
                    mockSearchEngine("bing"))))

            val manager = SearchEngineManager(listOf(provider))

            val shadow = ShadowApplication.getInstance()
            shadow.assertNoBroadcastListenersOfActionRegistered(
                    RuntimeEnvironment.application,
                    Intent.ACTION_LOCALE_CHANGED)

            manager.registerForLocaleUpdates(RuntimeEnvironment.application)

            assertTrue(shadow.registeredReceivers.find {
                it.intentFilter.hasAction(Intent.ACTION_LOCALE_CHANGED)
            } != null)

            verify(provider, never()).loadSearchEngines(RuntimeEnvironment.application)

            val context = ApplicationProvider.getApplicationContext<Context>()
            context.sendBroadcast(Intent(Intent.ACTION_LOCALE_CHANGED))
            launch(Dispatchers.Default) {}.join()

            verify(provider).loadSearchEngines(RuntimeEnvironment.application)
            verifyNoMoreInteractions(provider)
        }
    }

    private fun List<SearchEngine>.assertContainsIdentifier(identifier: String) {
        if (find { it.identifier == identifier } == null) {
            throw AssertionError("$identifier not in list")
        }
    }

    private fun mockProvider(engines: List<SearchEngine>): SearchEngineProvider =
            object : SearchEngineProvider {
                override suspend fun loadSearchEngines(context: Context): List<SearchEngine> {
                    return engines
                }
            }

    private fun mockSearchEngine(
        identifier: String,
        name: String = UUID.randomUUID().toString()
    ): SearchEngine {
        val uri = Uri.parse("https://${UUID.randomUUID()}.example.org")

        return SearchEngine(
            identifier,
            name,
            mock(Bitmap::class.java),
            listOf(uri))
    }
}
