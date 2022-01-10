/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.contile

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Response
import mozilla.components.support.test.any
import mozilla.components.support.test.file.loadResourceAsString
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import java.io.IOException

@RunWith(AndroidJUnit4::class)
class ContileTopSitesProviderTest {

    @Test
    fun `GIVEN a successful status response WHEN getTopSites is called THEN response should contain top sites`() = runBlocking {
        val client = prepareClient()
        val provider = ContileTopSitesProvider(client)
        val topSites = provider.getTopSites()
        var topSite = topSites.first()

        assertEquals(2, topSites.size)

        assertEquals(1L, topSite.id)
        assertEquals("Firefox", topSite.title)
        assertEquals("https://firefox.com", topSite.url)
        assertEquals("https://firefox.com/click", topSite.clickUrl)
        assertEquals("https://test.com/image1.jpg", topSite.imageUrl)
        assertEquals("https://test.com", topSite.impressionUrl)
        assertEquals(1, topSite.position)

        topSite = topSites.last()

        assertEquals(2L, topSite.id)
        assertEquals("Mozilla", topSite.title)
        assertEquals("https://mozilla.com", topSite.url)
        assertEquals("https://mozilla.com/click", topSite.clickUrl)
        assertEquals("https://test.com/image2.jpg", topSite.imageUrl)
        assertEquals("https://example.com", topSite.impressionUrl)
        assertEquals(2, topSite.position)
    }

    @Test(expected = IOException::class)
    fun `GIVEN a 500 status response WHEN getTopSites is called THEN throw an exception`() = runBlocking {
        val client = prepareClient(status = 500)
        val provider = ContileTopSitesProvider(client)
        provider.getTopSites()
        Unit
    }

    private fun prepareClient(
        jsonResponse: String = loadResourceAsString("/contile/contile.json"),
        status: Int = 200
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
