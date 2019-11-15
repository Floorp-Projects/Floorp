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
import mozilla.components.feature.addons.AddOn
import mozilla.components.support.test.any
import mozilla.components.support.test.file.loadResourceAsString
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import java.util.Date
import java.io.InputStream

@RunWith(AndroidJUnit4::class)
class AddOnCollectionProviderTest {

    @Test
    fun `getAvailableAddOns - with a successful status response must contain add-ons`() {
        val jsonResponse = loadResourceAsString("/collection.json")
        val mockedClient = mock<Client>()
        val mockedResponse = mock<Response>()
        val mockedBody = mock<Response.Body>()
        whenever(mockedBody.string(any())).thenReturn(jsonResponse)
        whenever(mockedResponse.body).thenReturn(mockedBody)
        whenever(mockedResponse.status).thenReturn(200)
        whenever(mockedClient.fetch(any())).thenReturn(mockedResponse)

        val provider = AddOnCollectionProvider(testContext, client = mockedClient)

        runBlocking {
            val addOns = provider.getAvailableAddOns()
            val addOn = addOns.first()

            assertTrue(addOns.isNotEmpty())

            // Add-on details
            assertEquals("607454", addOn.id)
            assertEquals("2015-04-25T07:26:22Z", addOn.createdAt)
            assertEquals("2019-10-24T09:28:41Z", addOn.updatedAt)
            assertEquals(
                "https://addons.cdn.mozilla.net/user-media/addon_icons/607/607454-64.png?modified=mcrushed",
                addOn.iconUrl
            )
            assertEquals(
                "https://addons.mozilla.org/en-US/firefox/addon/ublock-origin/",
                addOn.siteUrl
            )
            assertEquals(
                "https://addons.mozilla.org/firefox/downloads/file/3428595/ublock_origin-1.23.0-an+fx.xpi?src=",
                addOn.downloadUrl
            )
            assertEquals(
                "menus",
                addOn.permissions.first()
            )
            assertEquals(
                "uBlock Origin",
                addOn.translatableName["ca"]
            )
            assertEquals(
                "Finalment, un blocador eficient que utilitza pocs recursos de memòria i processador.",
                addOn.translatableSummary["ca"]
            )
            assertTrue(addOn.translatableDescription.getValue("ca").isNotBlank())
            assertEquals("1.23.0", addOn.version)

            // Authors
            assertEquals("11423598", addOn.authors.first().id)
            assertEquals("Raymond Hill", addOn.authors.first().name)
            assertEquals("gorhill", addOn.authors.first().username)
            assertEquals(
                "https://addons.mozilla.org/en-US/firefox/user/11423598/",
                addOn.authors.first().url
            )

            // Ratings
            assertEquals(4.7003F, addOn.rating!!.average, 0.7003F)
            assertEquals(9930, addOn.rating!!.reviews)
        }
    }

    @Test
    fun `getAvailableAddOns - with a successful status response must handle empty values`() {
        val jsonResponse = loadResourceAsString("/collection_with_empty_values.json")
        val mockedClient = mock<Client>()
        val mockedResponse = mock<Response>()
        val mockedMockedBody = mock<Response.Body>()

        whenever(mockedMockedBody.string(any())).thenReturn(jsonResponse)
        whenever(mockedResponse.body).thenReturn(mockedMockedBody)
        whenever(mockedResponse.status).thenReturn(200)
        whenever(mockedClient.fetch(any())).thenReturn(mockedResponse)

        val provider = AddOnCollectionProvider(testContext, client = mockedClient)

        runBlocking {
            val addOns = provider.getAvailableAddOns()
            val addOn = addOns.first()

            assertTrue(addOns.isNotEmpty())

            // Add-on
            assertEquals("", addOn.id)
            assertEquals("", addOn.createdAt)
            assertEquals("", addOn.updatedAt)
            assertEquals("", addOn.iconUrl)
            assertEquals("", addOn.siteUrl)
            assertEquals("", addOn.version)
            assertEquals("", addOn.downloadUrl)
            assertTrue(addOn.permissions.isEmpty())
            assertTrue(addOn.translatableName.isEmpty())
            assertTrue(addOn.translatableSummary.isEmpty())
            assertEquals("", addOn.translatableDescription.getValue("ca"))

            // Authors
            assertTrue(addOn.authors.isEmpty())
            verify(mockedClient).fetch(Request(url = "https://addons.mozilla.org/api/v4/accounts/account/mozilla/collections/7e8d6dc651b54ab385fb8791bf9dac/addons"))

            // Ratings
            assertNull(addOn.rating)
        }
    }

    @Test
    fun `getAvailableAddOns - with a not successful status will return an empty list`() {
        val mockedClient = mock<Client>()
        val mockedResponse = mock<Response>()
        val mockedMockedBody = mock<Response.Body>()

        whenever(mockedResponse.body).thenReturn(mockedMockedBody)
        whenever(mockedResponse.status).thenReturn(500)
        whenever(mockedClient.fetch(any())).thenReturn(mockedResponse)

        val provider = AddOnCollectionProvider(testContext, client = mockedClient)

        runBlocking {
            val addOns = provider.getAvailableAddOns()
            assertTrue(addOns.isEmpty())
        }
    }

    @Test
    fun `getAvailableAddOns - returns cached result only if allowed and not expired`() {
        val jsonResponse = loadResourceAsString("/collection.json")
        val mockedClient = mock<Client>()
        val mockedResponse = mock<Response>()
        val mockedBody = mock<Response.Body>()
        whenever(mockedBody.string(any())).thenReturn(jsonResponse)
        whenever(mockedResponse.body).thenReturn(mockedBody)
        whenever(mockedResponse.status).thenReturn(200)
        whenever(mockedClient.fetch(any())).thenReturn(mockedResponse)

        val provider = spy(AddOnCollectionProvider(testContext, client = mockedClient))

        runBlocking {
            provider.getAvailableAddOns(false)
            verify(provider, never()).readFromDiskCache()

            whenever(provider.cacheExpired(testContext)).thenReturn(true)
            provider.getAvailableAddOns(true)
            verify(provider, never()).readFromDiskCache()

            whenever(provider.cacheExpired(testContext)).thenReturn(false)
            provider.getAvailableAddOns(true)
            verify(provider).readFromDiskCache()
        }
    }

    @Test
    fun `getAvailableAddOns - writes response to cache if configured`() {
        val jsonResponse = loadResourceAsString("/collection.json")
        val mockedClient = mock<Client>()
        val mockedResponse = mock<Response>()
        val mockedBody = mock<Response.Body>()
        whenever(mockedBody.string(any())).thenReturn(jsonResponse)
        whenever(mockedResponse.body).thenReturn(mockedBody)
        whenever(mockedResponse.status).thenReturn(200)
        whenever(mockedClient.fetch(any())).thenReturn(mockedResponse)

        val provider = spy(AddOnCollectionProvider(testContext, client = mockedClient))
        val cachingProvider = spy(AddOnCollectionProvider(testContext, client = mockedClient, maxCacheAgeInMinutes = 1))

        runBlocking {
            provider.getAvailableAddOns()
            verify(provider, never()).writeToDiskCache(jsonResponse)

            cachingProvider.getAvailableAddOns()
            verify(cachingProvider).writeToDiskCache(jsonResponse)
        }
    }

    @Test
    fun `getAvailableAddOns - cache expiration check`() {
        var provider = spy(AddOnCollectionProvider(testContext, client = mock(), maxCacheAgeInMinutes = -1))
        whenever(provider.getCacheLastUpdated(testContext)).thenReturn(Date().time)
        assertTrue(provider.cacheExpired(testContext))

        whenever(provider.getCacheLastUpdated(testContext)).thenReturn(-1)
        assertTrue(provider.cacheExpired(testContext))

        provider = spy(AddOnCollectionProvider(testContext, client = mock(), maxCacheAgeInMinutes = 10))
        whenever(provider.getCacheLastUpdated(testContext)).thenReturn(-1)
        assertTrue(provider.cacheExpired(testContext))

        whenever(provider.getCacheLastUpdated(testContext)).thenReturn(Date().time - 60 * MINUTE_IN_MS)
        assertTrue(provider.cacheExpired(testContext))

        whenever(provider.getCacheLastUpdated(testContext)).thenReturn(Date().time + 60 * MINUTE_IN_MS)
        assertFalse(provider.cacheExpired(testContext))
    }

    @Test
    fun `getAddOnIconBitmap - with a successful status will return a bitmap`() {
        val mockedClient = mock<Client>()
        val mockedResponse = mock<Response>()
        val stream: InputStream = javaClass.getResourceAsStream("/png/mozac.png")!!.buffered()
        val responseBody = Response.Body(stream)

        whenever(mockedResponse.body).thenReturn(responseBody)
        whenever(mockedResponse.status).thenReturn(200)
        whenever(mockedClient.fetch(any())).thenReturn(mockedResponse)

        val provider = AddOnCollectionProvider(testContext, client = mockedClient)
        val addOn = AddOn(
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
            val bitmap = provider.getAddOnIconBitmap(addOn)
            assertTrue(bitmap is Bitmap)
        }
    }

    @Test
    fun `getAddOnIconBitmap - with an unsuccessful status will return null`() {
        val mockedClient = mock<Client>()
        val mockedResponse = mock<Response>()
        val mockedMockedBody = mock<Response.Body>()

        whenever(mockedResponse.body).thenReturn(mockedMockedBody)
        whenever(mockedResponse.status).thenReturn(500)
        whenever(mockedClient.fetch(any())).thenReturn(mockedResponse)

        val provider = AddOnCollectionProvider(testContext, client = mockedClient)
        val addOn = AddOn(
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
            val bitmap = provider.getAddOnIconBitmap(addOn)
            assertNull(bitmap)
        }
    }
}
