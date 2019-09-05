/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.search

import android.app.Application
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.graphics.Bitmap
import android.net.Uri
import androidx.test.core.app.ApplicationProvider.getApplicationContext
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.browser.search.provider.SearchEngineList
import mozilla.components.browser.search.provider.SearchEngineProvider
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions
import org.robolectric.Shadows.shadowOf
import java.util.UUID

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class SearchEngineManagerTest {

    @Test
    fun `manager returns engines from provider`() = runBlockingTest {
        val provider = mockProvider(listOf(
            mockSearchEngine("mozsearch"),
            mockSearchEngine("google"),
            mockSearchEngine("bing")))

        val manager = SearchEngineManager(listOf(provider),
            coroutineContext = coroutineContext)

        manager.loadAsync(testContext)
            .await()

        val verify = { engines: List<SearchEngine> ->
            assertEquals(3, engines.size)

            engines.assertContainsIdentifier("mozsearch")
            engines.assertContainsIdentifier("google")
            engines.assertContainsIdentifier("bing")
        }

        verify(manager.getSearchEngines(testContext))
        verify(manager.getSearchEnginesAsync(testContext))
    }

    @Test
    fun `manager returns engines from deferred`() = runBlockingTest {
        val provider = mockProvider(listOf(
            mockSearchEngine("mozsearch"),
            mockSearchEngine("google"),
            mockSearchEngine("bing")))

        val manager = SearchEngineManager(listOf(provider),
            coroutineContext = coroutineContext)

        val engines = manager.loadAsync(testContext)
            .await()
            .list

        assertEquals(3, engines.size)

        engines.assertContainsIdentifier("mozsearch")
        engines.assertContainsIdentifier("google")
        engines.assertContainsIdentifier("bing")
    }

    @Test
    fun `manager will load search engines on first get if not loaded previously`() = runBlockingTest {
        val provider = mockProvider(listOf(
            mockSearchEngine("mozsearch"),
            mockSearchEngine("google"),
            mockSearchEngine("bing")))

        val manager = SearchEngineManager(listOf(provider))

        val verify = { engines: List<SearchEngine> ->
            assertEquals(3, engines.size)
        }

        verify(manager.getSearchEngines(testContext))
        verify(manager.getSearchEnginesAsync(testContext))
    }

    @Test
    fun `manager returns first engine if default cannot be found`() = runBlockingTest {
        val provider = mockProvider(listOf(
            mockSearchEngine("mozsearch"),
            mockSearchEngine("google"),
            mockSearchEngine("bing")))

        val manager = SearchEngineManager(listOf(provider))

        val verify = { default: SearchEngine ->
            assertEquals("mozsearch", default.identifier)
        }

        verify(manager.getDefaultSearchEngine(testContext, "banana"))
        verify(manager.getDefaultSearchEngineAsync(testContext, "banana"))
    }

    @Test
    fun `manager returns default engine with identifier if it exists`() = runBlockingTest {
        val provider = mockProvider(listOf(
            mockSearchEngine("mozsearch", "Mozilla Search"),
            mockSearchEngine("google", "Google Search"),
            mockSearchEngine("bing", "Bing Search")))

        val manager = SearchEngineManager(listOf(provider))

        val verify = { default: SearchEngine ->
            assertEquals("bing", default.identifier)
        }

        verify(manager.getDefaultSearchEngine(testContext, "Bing Search"))
        verify(manager.getDefaultSearchEngineAsync(testContext, "Bing Search"))
    }

    @Test
    fun `manager returns default engine as default from the provider`() = runBlockingTest {
        val mozSearchEngine = mockSearchEngine("mozsearch")
        val provider = mockProvider(
            engines = listOf(
                mockSearchEngine("google"),
                mozSearchEngine,
                mockSearchEngine("bing")
            ),
            default = mozSearchEngine
        )

        val manager = SearchEngineManager(listOf(provider))

        val verify = { default: SearchEngine ->
            assertEquals("mozsearch", default.identifier)
        }

        verify(manager.getDefaultSearchEngine(testContext))
        verify(manager.getDefaultSearchEngineAsync(testContext))
    }

    @Test
    fun `manager returns first engine as default if no identifier is specified`() = runBlockingTest {
        val provider = mockProvider(listOf(
            mockSearchEngine("mozsearch"),
            mockSearchEngine("google"),
            mockSearchEngine("bing")))

        val manager = SearchEngineManager(listOf(provider))

        val verify = { default: SearchEngine ->
            assertEquals("mozsearch", default.identifier)
        }

        verify(manager.getDefaultSearchEngine(testContext))
        verify(manager.getDefaultSearchEngineAsync(testContext))
    }

    @Test
    fun `manager returns set default engine as default when no identifier is specified`() = runBlockingTest {
        val provider = mockProvider(
            listOf(
                mockSearchEngine("mozsearch"),
                mockSearchEngine("google"),
                mockSearchEngine("bing")
            )
        )

        val manager = SearchEngineManager(listOf(provider))
        manager.defaultSearchEngine = mockSearchEngine("bing")

        val verify = { default: SearchEngine ->
            assertEquals("bing", default.identifier)
        }

        verify(manager.getDefaultSearchEngine(testContext))
        verify(manager.getDefaultSearchEngineAsync(testContext))
    }

    @Test
    fun `manager returns engine from identifier as default when identifier is specified`() = runBlockingTest {
        val provider = mockProvider(
            listOf(
                mockSearchEngine("mozsearch", "Mozilla Search"),
                mockSearchEngine("google", "Google Search"),
                mockSearchEngine("bing", "Bing Search")
            )
        )

        val manager = SearchEngineManager(listOf(provider))
        manager.defaultSearchEngine = mockSearchEngine("bing")

        val verify = { default: SearchEngine ->
            assertEquals("google", default.identifier)
        }

        verify(manager.getDefaultSearchEngine(testContext, "Google Search"))
        verify(manager.getDefaultSearchEngineAsync(testContext, "Google Search"))
    }

    @Test
    fun `manager returns first engine as provided default if default cannot be found`() = runBlockingTest {
        val provider = mockProvider(listOf(
                mockSearchEngine("mozsearch"),
                mockSearchEngine("google"),
                mockSearchEngine("bing")))

        val manager = SearchEngineManager(listOf(provider))

        val verify = { default: SearchEngine ->
            assertEquals("mozsearch", default.identifier)
        }

        verify(manager.getProvidedDefaultSearchEngine(testContext))
        verify(manager.getProvidedDefaultSearchEngineAsync(testContext))
    }

    @Test
    fun `manager returns default engine as provided default from the provider`() = runBlockingTest {
        val mozSearchEngine = mockSearchEngine("mozsearch")
        val provider = mockProvider(
            engines = listOf(
                mockSearchEngine("google"),
                mozSearchEngine,
                mockSearchEngine("bing")
            ),
            default = mozSearchEngine
        )

        val manager = SearchEngineManager(listOf(provider))

        val verify = { default: SearchEngine ->
            assertEquals("mozsearch", default.identifier)
        }

        verify(manager.getProvidedDefaultSearchEngine(testContext))
        verify(manager.getProvidedDefaultSearchEngineAsync(testContext))
    }

    @Test
    fun `manager registers for locale changes`() = runBlockingTest {
        val provider = spy(mockProvider(listOf(
            mockSearchEngine("mozsearch"),
            mockSearchEngine("google"),
            mockSearchEngine("bing"))))

        val manager = SearchEngineManager(listOf(provider))

        val context = spy(testContext)

        manager.registerForLocaleUpdates(context)
        val intentFilter = argumentCaptor<IntentFilter>()
        verify(context).registerReceiver(eq(manager.localeChangedReceiver), intentFilter.capture())

        intentFilter.value.hasAction(Intent.ACTION_LOCALE_CHANGED)
    }

    @Test
    fun `locale update triggers load`() = runBlockingTest {
        val provider = spy(mockProvider(listOf(
            mockSearchEngine("mozsearch"),
            mockSearchEngine("google"),
            mockSearchEngine("bing"))))

        val manager = spy(SearchEngineManager(listOf(provider), coroutineContext))
        manager.localeChangedReceiver.onReceive(testContext, mock())

        verify(provider).loadSearchEngines(testContext)
        verifyNoMoreInteractions(provider)
    }

    @Test
    fun `load calls providers loadSearchEngine`() = runBlockingTest {
        val provider = spy(mockProvider(listOf(
            mockSearchEngine("mozsearch"),
            mockSearchEngine("google"),
            mockSearchEngine("bing"))))

        val manager = spy(SearchEngineManager(listOf(provider), coroutineContext))
        manager.loadAsync(testContext).await()

        verify(provider).loadSearchEngines(testContext)
        verifyNoMoreInteractions(provider)
    }

    @Test
    fun `locale update broadcast will trigger reload`() = runBlockingTest {
        val provider = spy(mockProvider(listOf(
            mockSearchEngine("mozsearch"),
            mockSearchEngine("google"),
            mockSearchEngine("bing"))))

        val manager = SearchEngineManager(listOf(provider), coroutineContext)

        val shadow = shadowOf(getApplicationContext<Application>())
        shadow.assertNoBroadcastListenersOfActionRegistered(
            getApplicationContext(),
            Intent.ACTION_LOCALE_CHANGED)

        manager.registerForLocaleUpdates(testContext)

        assertTrue(shadow.registeredReceivers.find {
            it.intentFilter.hasAction(Intent.ACTION_LOCALE_CHANGED)
        } != null)

        verify(provider, never()).loadSearchEngines(testContext)

        val context = getApplicationContext<Context>()
        context.sendBroadcast(Intent(Intent.ACTION_LOCALE_CHANGED))

        verify(provider).loadSearchEngines(testContext)
        verifyNoMoreInteractions(provider)
    }

    private fun List<SearchEngine>.assertContainsIdentifier(identifier: String) {
        if (find { it.identifier == identifier } == null) {
            throw AssertionError("$identifier not in list")
        }
    }

    private fun mockProvider(engines: List<SearchEngine>, default: SearchEngine? = null) =
        object : SearchEngineProvider {
            override suspend fun loadSearchEngines(context: Context): SearchEngineList {
                return SearchEngineList(engines, default)
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
