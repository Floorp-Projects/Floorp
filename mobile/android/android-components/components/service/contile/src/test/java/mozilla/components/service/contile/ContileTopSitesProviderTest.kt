/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.contile

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Response
import mozilla.components.support.test.any
import mozilla.components.support.test.file.loadResourceAsString
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
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
    fun `GIVEN a successful status response WHEN top sites are fetched THEN response should contain top sites`() = runTest {
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
    fun `GIVEN a 500 status response WHEN top sites are fetched THEN throw an exception`() = runTest {
        val client = prepareClient(status = 500)
        val provider = ContileTopSitesProvider(testContext, client)
        provider.getTopSites()
        Unit
    }

    @Test
    fun `GIVEN a cache configuration is allowed and not expired WHEN top sites are fetched THEN read from the disk cache`() = runTest {
        val client = prepareClient()
        val provider = spy(ContileTopSitesProvider(testContext, client))

        provider.getTopSites(allowCache = false)
        verify(provider, never()).readFromDiskCache()

        whenever(provider.isCacheExpired()).thenReturn(true)
        provider.getTopSites(allowCache = true)
        verify(provider, never()).readFromDiskCache()

        whenever(provider.isCacheExpired()).thenReturn(false)
        provider.getTopSites(allowCache = true)
        verify(provider).readFromDiskCache()

        Unit
    }

    @Test
    fun `GIVEN a cache configuration is allowed WHEN top sites are fetched THEN write response to cache`() = runTest {
        val jsonResponse = loadResourceAsString("/contile/contile.json")
        val client = prepareClient(jsonResponse)
        val provider = spy(ContileTopSitesProvider(testContext, client))
        val cachingProvider = spy(
            ContileTopSitesProvider(
                context = testContext,
                client = client,
                maxCacheAgeInMinutes = 1L,
            ),
        )

        assertNull(provider.diskCacheLastModified)
        assertNull(cachingProvider.diskCacheLastModified)

        provider.getTopSites()
        verify(provider, never()).writeToDiskCache(jsonResponse)
        assertNull(provider.diskCacheLastModified)

        cachingProvider.getTopSites()
        verify(cachingProvider).writeToDiskCache(jsonResponse)
        assertNotNull(cachingProvider.diskCacheLastModified)
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
    fun `GIVEN a max cache age WHEN the cache expiration is checked THEN return whether the cache is expired`() {
        var provider =
            spy(ContileTopSitesProvider(testContext, client = mock(), maxCacheAgeInMinutes = -1))

        whenever(provider.getCacheLastModified()).thenReturn(Date().time)
        assertTrue(provider.isCacheExpired())

        whenever(provider.getCacheLastModified()).thenReturn(-1)
        assertTrue(provider.isCacheExpired())

        provider =
            spy(ContileTopSitesProvider(testContext, client = mock(), maxCacheAgeInMinutes = 10))

        whenever(provider.getCacheLastModified()).thenReturn(-1)
        assertTrue(provider.isCacheExpired())

        whenever(provider.getCacheLastModified()).thenReturn(Date().time - 60 * MINUTE_IN_MS)
        assertTrue(provider.isCacheExpired())

        whenever(provider.getCacheLastModified()).thenReturn(Date().time)
        assertFalse(provider.isCacheExpired())

        whenever(provider.getCacheLastModified()).thenReturn(Date().time + 60 * MINUTE_IN_MS)
        assertFalse(provider.isCacheExpired())
    }

    @Test
    fun `WHEN the cache last modified time is fetched THEN the returned value is cached`() {
        val provider = spy(ContileTopSitesProvider(testContext, prepareClient()))
        val file = File(testContext.filesDir, CACHE_FILE_NAME)

        assertNull(provider.diskCacheLastModified)

        file.createNewFile()

        assertTrue(file.exists())

        var cacheLastModified = provider.getCacheLastModified()

        assertEquals(cacheLastModified, provider.diskCacheLastModified)
        assertTrue(file.delete())

        cacheLastModified = provider.getCacheLastModified()

        assertEquals(cacheLastModified, provider.diskCacheLastModified)
    }

    @Test
    fun `GIVEN cache is not expired WHEN top sites are refreshed THEN do nothing`() = runTest {
        val provider = spy(
            ContileTopSitesProvider(
                testContext,
                client = prepareClient(),
                maxCacheAgeInMinutes = 10,
            ),
        )

        whenever(provider.isCacheExpired()).thenReturn(false)
        provider.refreshTopSitesIfCacheExpired()
        verify(provider, never()).getTopSites(allowCache = false)

        Unit
    }

    @Test
    fun `GIVEN cache is expired WHEN top sites are refreshed THEN fetch and write new response to cache`() = runTest {
        val jsonResponse = loadResourceAsString("/contile/contile.json")
        val provider = spy(
            ContileTopSitesProvider(
                testContext,
                client = prepareClient(jsonResponse),
                maxCacheAgeInMinutes = 10,
            ),
        )

        whenever(provider.isCacheExpired()).thenReturn(true)

        provider.refreshTopSitesIfCacheExpired()

        verify(provider).getTopSites(allowCache = false)
        verify(provider).writeToDiskCache(jsonResponse)
    }

    private fun prepareClient(
        jsonResponse: String = loadResourceAsString("/contile/contile.json"),
        status: Int = 200,
    ): Client {
        val mockedClient = mock<Client>()
        val mockedResponse = mock<Response>()
        val mockedBody = mock<Response.Body>()

        whenever(mockedBody.string(any())).thenReturn(jsonResponse)
        whenever(mockedResponse.body).thenReturn(mockedBody)
        whenever(mockedResponse.status).thenReturn(status)
        whenever(mockedClient.fetch(any())).thenReturn(mockedResponse)

        return mockedClient
    }
}
