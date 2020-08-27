/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.amo

import android.graphics.Bitmap
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.feature.addons.Addon
import mozilla.components.support.test.any
import mozilla.components.support.test.file.loadResourceAsString
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import java.io.IOException
import java.util.Date
import java.io.InputStream
import java.util.concurrent.TimeUnit

@RunWith(AndroidJUnit4::class)
class AddonCollectionProviderTest {

    @Test
    fun `getAvailableAddons - with a successful status response must contain add-ons`() {
        val jsonResponse = loadResourceAsString("/collection.json")
        val mockedClient = mock<Client>()
        val mockedResponse = mock<Response>()
        val mockedBody = mock<Response.Body>()
        whenever(mockedBody.string(any())).thenReturn(jsonResponse)
        whenever(mockedResponse.body).thenReturn(mockedBody)
        whenever(mockedResponse.status).thenReturn(200)
        whenever(mockedClient.fetch(any())).thenReturn(mockedResponse)

        val provider = AddonCollectionProvider(testContext, client = mockedClient)

        runBlocking {
            val addons = provider.getAvailableAddons()
            val addon = addons.first()

            assertTrue(addons.isNotEmpty())

            // Add-on details
            assertEquals("uBlock0@raymondhill.net", addon.id)
            assertEquals("2015-04-25T07:26:22Z", addon.createdAt)
            assertEquals("2019-10-24T09:28:41Z", addon.updatedAt)
            assertEquals(
                "https://addons.cdn.mozilla.net/user-media/addon_icons/607/607454-64.png?modified=mcrushed",
                addon.iconUrl
            )
            assertEquals(
                "https://addons.mozilla.org/en-US/firefox/addon/ublock-origin/",
                addon.siteUrl
            )
            assertEquals(
                "https://addons.mozilla.org/firefox/downloads/file/3428595/ublock_origin-1.23.0-an+fx.xpi?src=",
                addon.downloadUrl
            )
            assertEquals(
                "menus",
                addon.permissions.first()
            )
            assertEquals(
                "uBlock Origin",
                addon.translatableName["ca"]
            )
            assertEquals(
                "Finalment, un blocador eficient que utilitza pocs recursos de memòria i processador.",
                addon.translatableSummary["ca"]
            )
            assertTrue(addon.translatableDescription.getValue("ca").isNotBlank())
            assertEquals("1.23.0", addon.version)
            assertEquals("es", addon.defaultLocale)

            // Authors
            assertEquals("11423598", addon.authors.first().id)
            assertEquals("Raymond Hill", addon.authors.first().name)
            assertEquals("gorhill", addon.authors.first().username)
            assertEquals(
                "https://addons.mozilla.org/en-US/firefox/user/11423598/",
                addon.authors.first().url
            )

            // Ratings
            assertEquals(4.7003F, addon.rating!!.average, 0.7003F)
            assertEquals(9930, addon.rating!!.reviews)
        }
    }

    @Test
    fun `getAvailableAddons - with a successful status response must handle empty values`() {
        val jsonResponse = loadResourceAsString("/collection_with_empty_values.json")
        val mockedClient = mock<Client>()
        val mockedResponse = mock<Response>()
        val mockedMockedBody = mock<Response.Body>()

        whenever(mockedMockedBody.string(any())).thenReturn(jsonResponse)
        whenever(mockedResponse.body).thenReturn(mockedMockedBody)
        whenever(mockedResponse.status).thenReturn(200)
        whenever(mockedClient.fetch(any())).thenReturn(mockedResponse)

        val provider = AddonCollectionProvider(testContext, client = mockedClient)

        runBlocking {
            val addons = provider.getAvailableAddons()
            val addon = addons.first()

            assertTrue(addons.isNotEmpty())

            // Add-on
            assertEquals("", addon.id)
            assertEquals("", addon.createdAt)
            assertEquals("", addon.updatedAt)
            assertEquals("", addon.iconUrl)
            assertEquals("", addon.siteUrl)
            assertEquals("", addon.version)
            assertEquals("", addon.downloadUrl)
            assertTrue(addon.permissions.isEmpty())
            assertTrue(addon.translatableName.isEmpty())
            assertTrue(addon.translatableSummary.isEmpty())
            assertEquals("", addon.translatableDescription.getValue("ca"))
            assertEquals(Addon.DEFAULT_LOCALE, addon.defaultLocale)

            // Authors
            assertTrue(addon.authors.isEmpty())
            verify(mockedClient).fetch(Request(
                url = "https://addons.mozilla.org/api/v4/accounts/account/mozilla/collections/7e8d6dc651b54ab385fb8791bf9dac/addons",
                readTimeout = Pair(DEFAULT_READ_TIMEOUT_IN_SECONDS, TimeUnit.SECONDS)
            ))

            // Ratings
            assertNull(addon.rating)
        }
    }

    @Test
    fun `getAvailableAddons - read timeout can be configured`() {
        val jsonResponse = loadResourceAsString("/collection_with_empty_values.json")
        val mockedClient = mock<Client>()
        val mockedResponse = mock<Response>()
        val mockedBody = mock<Response.Body>()
        whenever(mockedBody.string(any())).thenReturn(jsonResponse)
        whenever(mockedResponse.body).thenReturn(mockedBody)
        whenever(mockedResponse.status).thenReturn(200)
        whenever(mockedClient.fetch(any())).thenReturn(mockedResponse)

        val provider = spy(AddonCollectionProvider(testContext, client = mockedClient))

        runBlocking {
            provider.getAvailableAddons(readTimeoutInSeconds = 5)
            verify(mockedClient).fetch(Request(
                url = "https://addons.mozilla.org/api/v4/accounts/account/mozilla/collections/7e8d6dc651b54ab385fb8791bf9dac/addons",
                readTimeout = Pair(5, TimeUnit.SECONDS)
            ))
        }
    }

    @Test(expected = IOException::class)
    fun `getAvailableAddons - with unexpected status will throw exception`() {
        val mockedClient = mock<Client>()
        val mockedResponse = mock<Response>()
        val mockedMockedBody = mock<Response.Body>()

        whenever(mockedResponse.body).thenReturn(mockedMockedBody)
        whenever(mockedResponse.status).thenReturn(500)
        whenever(mockedClient.fetch(any())).thenReturn(mockedResponse)

        val provider = AddonCollectionProvider(testContext, client = mockedClient)

        runBlocking {
            provider.getAvailableAddons()
        }
    }

    @Test
    fun `getAvailableAddons - returns cached result if allowed and not expired`() {
        val jsonResponse = loadResourceAsString("/collection.json")
        val mockedClient = mock<Client>()
        val mockedResponse = mock<Response>()
        val mockedBody = mock<Response.Body>()
        whenever(mockedBody.string(any())).thenReturn(jsonResponse)
        whenever(mockedResponse.body).thenReturn(mockedBody)
        whenever(mockedResponse.status).thenReturn(200)
        whenever(mockedClient.fetch(any())).thenReturn(mockedResponse)

        val provider = spy(AddonCollectionProvider(testContext, client = mockedClient))

        runBlocking {
            provider.getAvailableAddons(false)
            verify(provider, never()).readFromDiskCache()

            whenever(provider.cacheExpired(testContext)).thenReturn(true)
            provider.getAvailableAddons(true)
            verify(provider, never()).readFromDiskCache()

            whenever(provider.cacheExpired(testContext)).thenReturn(false)
            provider.getAvailableAddons(true)
            verify(provider).readFromDiskCache()
        }
    }

    @Test
    fun `getAvailableAddons - returns cached result if allowed and fetch failed`() {
        val mockedClient: Client = mock()
        val exception = IOException("test")
        val cachedAddons: List<Addon> = emptyList()
        whenever(mockedClient.fetch(any())).thenThrow(exception)

        val provider = spy(AddonCollectionProvider(testContext, client = mockedClient))

        runBlocking {
            try {
                // allowCache = false
                provider.getAvailableAddons(allowCache = false)
                fail("Expected IOException")
            } catch (e: IOException) {
                assertSame(exception, e)
            }

            try {
                // allowCache = true, but no cache present
                provider.getAvailableAddons(allowCache = true)
                fail("Expected IOException")
            } catch (e: IOException) {
                assertSame(exception, e)
            }

            try {
                // allowCache = true, cache present, but we fail to read
                whenever(provider.getCacheLastUpdated(testContext)).thenReturn(Date().time)
                provider.getAvailableAddons(allowCache = true)
                fail("Expected IOException")
            } catch (e: IOException) {
                assertSame(exception, e)
            }

            // allowCache = true, cache present, and reading successfully
            whenever(provider.getCacheLastUpdated(testContext)).thenReturn(Date().time)
            whenever(provider.readFromDiskCache()).thenReturn(cachedAddons)
            assertSame(cachedAddons, provider.getAvailableAddons(allowCache = true))
        }
    }

    @Test
    fun `getAvailableAddons - writes response to cache if configured`() {
        val jsonResponse = loadResourceAsString("/collection.json")
        val mockedClient = mock<Client>()
        val mockedResponse = mock<Response>()
        val mockedBody = mock<Response.Body>()
        whenever(mockedBody.string(any())).thenReturn(jsonResponse)
        whenever(mockedResponse.body).thenReturn(mockedBody)
        whenever(mockedResponse.status).thenReturn(200)
        whenever(mockedClient.fetch(any())).thenReturn(mockedResponse)

        val provider = spy(AddonCollectionProvider(testContext, client = mockedClient))
        val cachingProvider = spy(AddonCollectionProvider(testContext, client = mockedClient, maxCacheAgeInMinutes = 1))

        runBlocking {
            provider.getAvailableAddons()
            verify(provider, never()).writeToDiskCache(jsonResponse)

            cachingProvider.getAvailableAddons()
            verify(cachingProvider).writeToDiskCache(jsonResponse)
        }
    }

    @Test
    fun `getAvailableAddons - cache expiration check`() {
        var provider = spy(AddonCollectionProvider(testContext, client = mock(), maxCacheAgeInMinutes = -1))
        whenever(provider.getCacheLastUpdated(testContext)).thenReturn(Date().time)
        assertTrue(provider.cacheExpired(testContext))

        whenever(provider.getCacheLastUpdated(testContext)).thenReturn(-1)
        assertTrue(provider.cacheExpired(testContext))

        provider = spy(AddonCollectionProvider(testContext, client = mock(), maxCacheAgeInMinutes = 10))
        whenever(provider.getCacheLastUpdated(testContext)).thenReturn(-1)
        assertTrue(provider.cacheExpired(testContext))

        whenever(provider.getCacheLastUpdated(testContext)).thenReturn(Date().time - 60 * MINUTE_IN_MS)
        assertTrue(provider.cacheExpired(testContext))

        whenever(provider.getCacheLastUpdated(testContext)).thenReturn(Date().time + 60 * MINUTE_IN_MS)
        assertFalse(provider.cacheExpired(testContext))
    }

    @Test
    fun `getAddonIconBitmap - with a successful status will return a bitmap`() {
        val mockedClient = mock<Client>()
        val mockedResponse = mock<Response>()
        val stream: InputStream = javaClass.getResourceAsStream("/png/mozac.png")!!.buffered()
        val responseBody = Response.Body(stream)

        whenever(mockedResponse.body).thenReturn(responseBody)
        whenever(mockedResponse.status).thenReturn(200)
        whenever(mockedClient.fetch(any())).thenReturn(mockedResponse)

        val provider = AddonCollectionProvider(testContext, client = mockedClient)
        val addon = Addon(
                id = "id",
                authors = mock(),
                categories = mock(),
                downloadUrl = "https://example.com",
                version = "version",
                iconUrl = "https://example.com/image.png",
                createdAt = "0",
                updatedAt = "0"
        )

        runBlocking {
            val bitmap = provider.getAddonIconBitmap(addon)
            assertTrue(bitmap is Bitmap)
        }
    }

    @Test
    fun `getAddonIconBitmap - with an unsuccessful status will return null`() {
        val mockedClient = mock<Client>()
        val mockedResponse = mock<Response>()
        val mockedMockedBody = mock<Response.Body>()

        whenever(mockedResponse.body).thenReturn(mockedMockedBody)
        whenever(mockedResponse.status).thenReturn(500)
        whenever(mockedClient.fetch(any())).thenReturn(mockedResponse)

        val provider = AddonCollectionProvider(testContext, client = mockedClient)
        val addon = Addon(
                id = "id",
                authors = mock(),
                categories = mock(),
                downloadUrl = "https://example.com",
                version = "version",
                iconUrl = "https://example.com/image.png",
                createdAt = "0",
                updatedAt = "0"
        )

        runBlocking {
            val bitmap = provider.getAddonIconBitmap(addon)
            assertNull(bitmap)
        }
    }
}
