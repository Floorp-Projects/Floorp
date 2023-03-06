/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.contile

import android.text.format.DateUtils
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Header
import mozilla.components.concept.fetch.Headers
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Response
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.file.loadResourceAsString
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.anyLong
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import java.io.File
import java.io.IOException
import java.util.Date

@ExperimentalCoroutinesApi // for runTest
@RunWith(AndroidJUnit4::class)
class ContileTopSitesProviderTest {

    @Test
    fun `GIVEN a successful status response WHEN top sites are fetched THEN response should contain top sites`() =
        runTest {
            val client = prepareClient()
            val provider = ContileTopSitesProvider(testContext, client)
            val topSites = provider.getTopSites()
            var topSite = topSites.first()

            assertEquals(2, topSites.size)
            assertEquals(1L, topSite.id)
            assertEquals("Firefox", topSite.title)
            assertEquals("https://firefox.com", topSite.url)
            assertEquals("https://firefox.com/click", topSite.clickUrl)
            assertEquals("https://test.com/image1.jpg", topSite.imageUrl)
            assertEquals("https://test.com", topSite.impressionUrl)

            topSite = topSites.last()

            assertEquals(2L, topSite.id)
            assertEquals("Mozilla", topSite.title)
            assertEquals("https://mozilla.com", topSite.url)
            assertEquals("https://mozilla.com/click", topSite.clickUrl)
            assertEquals("https://test.com/image2.jpg", topSite.imageUrl)
            assertEquals("https://example.com", topSite.impressionUrl)
        }

    @Test(expected = IOException::class)
    fun `GIVEN a 500 status response WHEN top sites are fetched AND cached top sites are not valid THEN throw an exception`() =
        runTest {
            val client = prepareClient(status = 500)
            val provider = ContileTopSitesProvider(testContext, client)
            provider.getTopSites()
        }

    @Test
    fun `GIVEN a 500 status response WHEN top sites are fetched AND cached top sites are valid THEN return the cached top sites`() =
        runTest {
            val client = prepareClient(status = 500)
            val topSites = mock<List<TopSite.Provided>>()
            val provider =
                spy(ContileTopSitesProvider(testContext, client, maxCacheAgeInSeconds = 60L))
            whenever(provider.isCacheExpired(false)).thenReturn(true)
            whenever(provider.isCacheExpired(true)).thenReturn(false)
            whenever(provider.readFromDiskCache()).thenReturn(
                ContileTopSitesProvider.CachedData(
                    60L,
                    topSites,
                ),
            )

            assertEquals(topSites, provider.getTopSites())
        }

    @Test
    fun `GIVEN a cache configuration is allowed and not expired WHEN top sites are fetched THEN read from the disk cache`() =
        runTest {
            val client = prepareClient()
            val provider = spy(ContileTopSitesProvider(testContext, client))

            provider.getTopSites(allowCache = false)
            verify(provider, never()).readFromDiskCache()

            whenever(provider.isCacheExpired(false)).thenReturn(true)
            provider.getTopSites(allowCache = true)
            verify(provider, never()).readFromDiskCache()

            whenever(provider.isCacheExpired(false)).thenReturn(false)
            provider.getTopSites(allowCache = true)
            verify(provider).readFromDiskCache()
        }

    @Test
    fun `GIVEN a cache max age is specified WHEN top sites are fetched THEN the cache max age is correctly set`() =
        runTest {
            val jsonResponse = loadResourceAsString("/contile/contile.json")
            val client = prepareClient(jsonResponse)
            val specifiedProvider = spy(
                ContileTopSitesProvider(
                    context = testContext,
                    client = client,
                    maxCacheAgeInSeconds = 60L,
                ),
            )

            specifiedProvider.getTopSites()
            verify(specifiedProvider).writeToDiskCache(anyLong(), any())
            assertEquals(
                specifiedProvider.cacheState.localCacheMaxAge,
                specifiedProvider.cacheState.getCacheMaxAge(),
            )
            assertFalse(specifiedProvider.isCacheExpired(false))
        }

    @Test
    fun `GIVEN cache max age is not specified WHEN top sites are fetched THEN the cache is expired`() =
        runTest {
            val jsonResponse = loadResourceAsString("/contile/contile.json")
            val client = prepareClient(jsonResponse)
            val provider = spy(ContileTopSitesProvider(testContext, client))

            provider.getTopSites()
            verify(provider).writeToDiskCache(anyLong(), any())
            assertTrue(provider.isCacheExpired(false))
        }

    @Test
    fun `WHEN the base cache file getter is called THEN return existing base cache file`() {
        val client = prepareClient()
        val provider = spy(ContileTopSitesProvider(testContext, client))
        val file = File(testContext.filesDir, CACHE_FILE_NAME)

        file.createNewFile()

        assertTrue(file.exists())

        val cacheFile = provider.getBaseCacheFile()

        assertTrue(cacheFile.exists())
        assertEquals(file.name, cacheFile.name)

        assertTrue(file.delete())
        assertFalse(cacheFile.exists())
    }

    @Test
    fun `WHEN the cache expiration is checked THEN return whether the cache is expired`() {
        val provider =
            spy(ContileTopSitesProvider(testContext, client = mock()))

        provider.cacheState =
            ContileTopSitesProvider.CacheState(isCacheValid = false)
        assertTrue(provider.isCacheExpired(false))

        provider.cacheState = ContileTopSitesProvider.CacheState(
            isCacheValid = true,
            localCacheMaxAge = Date().time - 60 * DateUtils.MINUTE_IN_MILLIS,
        )
        assertTrue(provider.isCacheExpired(false))

        provider.cacheState = ContileTopSitesProvider.CacheState(
            isCacheValid = true,
            localCacheMaxAge = Date().time + 60 * DateUtils.MINUTE_IN_MILLIS,
        )
        assertFalse(provider.isCacheExpired(false))

        provider.cacheState = ContileTopSitesProvider.CacheState(
            isCacheValid = true,
            serverCacheMaxAge = Date().time - 60 * DateUtils.MINUTE_IN_MILLIS,
        )
        assertTrue(provider.isCacheExpired(true))

        provider.cacheState = ContileTopSitesProvider.CacheState(
            isCacheValid = true,
            serverCacheMaxAge = Date().time + 60 * DateUtils.MINUTE_IN_MILLIS,
        )
        assertFalse(provider.isCacheExpired(true))
    }

    @Test
    fun `GIVEN cache is not expired WHEN top sites are refreshed THEN do nothing`() = runTest {
        val provider = spy(
            ContileTopSitesProvider(
                testContext,
                client = prepareClient(),
                maxCacheAgeInSeconds = 600,
            ),
        )

        whenever(provider.isCacheExpired(false)).thenReturn(false)
        provider.refreshTopSitesIfCacheExpired()
        verify(provider, never()).getTopSites(allowCache = false)
    }

    @Test
    fun `GIVEN cache is expired WHEN top sites are refreshed THEN fetch and write new response to cache`() =
        runTest {
            val jsonResponse = loadResourceAsString("/contile/contile.json")
            val provider = spy(
                ContileTopSitesProvider(
                    testContext,
                    client = prepareClient(jsonResponse),
                ),
            )

            whenever(provider.isCacheExpired(false)).thenReturn(true)

            provider.refreshTopSitesIfCacheExpired()

            verify(provider).getTopSites(allowCache = false)
            verify(provider).writeToDiskCache(eq(300000L), any())
        }

    @Test
    fun `GIVEN a NO_CONTENT status response WHEN top sites are fetched THEN cache is cleared`() = runTest {
        val client = prepareClient(status = Response.NO_CONTENT)
        val provider = spy(ContileTopSitesProvider(testContext, client))
        val file = mock<File>()

        whenever(provider.isCacheExpired(false)).thenReturn(true)
        whenever(provider.getBaseCacheFile()).thenReturn(file)
        provider.refreshTopSitesIfCacheExpired()

        verify(file).delete()
        assertNull(provider.cacheState.localCacheMaxAge)
        assertNull(provider.cacheState.serverCacheMaxAge)
    }

    private fun prepareClient(
        jsonResponse: String = loadResourceAsString("/contile/contile.json"),
        status: Int = 200,
        headers: Headers = MutableHeaders(
            listOf(
                Header("cache-control", "max-age=100"),
                Header("cache-control", "stale-if-error=200"),
            ),
        ),
    ): Client {
        val mockedClient = mock<Client>()
        val mockedResponse = mock<Response>()
        val mockedBody = mock<Response.Body>()

        whenever(mockedBody.string(any())).thenReturn(jsonResponse)
        whenever(mockedResponse.body).thenReturn(mockedBody)
        whenever(mockedResponse.status).thenReturn(status)
        whenever(mockedResponse.headers).thenReturn(headers)
        whenever(mockedClient.fetch(any())).thenReturn(mockedResponse)

        return mockedClient
    }

    @Test
    fun `WHEN extracting top sites from a json object contains top sites THEN all top sites are correctly set`() {
        val topSites = with(
            TopSite.Provided(
                1,
                "firefox",
                "www.mozilla.com",
                "www.mozilla.com",
                "www.mozilla.com",
                "www.mozilla.com",
                null,
            ),
        ) {
            listOf(this, this.copy(id = 2))
        }

        val jsonObject = JSONObject(
            mapOf(
                CACHE_TOP_SITES_KEY to JSONArray().also { array ->
                    topSites.map { it.toJsonObject() }.forEach { array.put(it) }
                },
            ),
        )

        assertEquals(topSites, jsonObject.getTopSites())
    }

    private fun TopSite.Provided.toJsonObject() =
        JSONObject()
            .put("id", id)
            .put("name", title)
            .put("url", url)
            .put("click_url", clickUrl)
            .put("image_url", imageUrl)
            .put("impression_url", impressionUrl)
}
