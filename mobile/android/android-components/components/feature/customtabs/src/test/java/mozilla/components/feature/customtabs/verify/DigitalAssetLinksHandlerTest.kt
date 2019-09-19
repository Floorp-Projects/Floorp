/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs.verify

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Response
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn

@RunWith(AndroidJUnit4::class)
@ExperimentalCoroutinesApi
class DigitalAssetLinksHandlerTest {

    private val apiKey = "XXXXXXXXXXX"
    private lateinit var client: Client
    private lateinit var handler: DigitalAssetLinksHandler

    @Before
    fun setup() {
        client = mock()
        handler = DigitalAssetLinksHandler(client, apiKey)
    }

    @Test
    fun `reject for invalid status`() {
        val response: Response = mock()
        doReturn(400).`when`(response).status
        doReturn(response).`when`(client).fetch(any())

        assertFalse(handler.checkDigitalAssetLinkRelationship("", "", "", ""))
    }

    @Test
    fun `reject for invalid json`() {
        doReturn(mockResponse("")).`when`(client).fetch(any())
        assertFalse(handler.checkDigitalAssetLinkRelationship("", "", "", ""))

        doReturn(mockResponse("{}")).`when`(client).fetch(any())
        assertFalse(handler.checkDigitalAssetLinkRelationship("", "", "", ""))

        doReturn(mockResponse("[]")).`when`(client).fetch(any())
        assertFalse(handler.checkDigitalAssetLinkRelationship("", "", "", ""))

        doReturn(mockResponse("{\"lnkd\":true}")).`when`(client).fetch(any())
        assertFalse(handler.checkDigitalAssetLinkRelationship("", "", "", ""))
    }

    @Test
    fun `return linked from json`() {
        doReturn(mockResponse("{\"linked\":true}")).`when`(client).fetch(any())
        assertTrue(handler.checkDigitalAssetLinkRelationship("", "", "", ""))

        doReturn(mockResponse("{\"linked\":true,\"maxAge\":\"3s\"}")).`when`(client).fetch(any())
        assertTrue(handler.checkDigitalAssetLinkRelationship("", "", "", ""))

        doReturn(mockResponse("{\"linked\":false}")).`when`(client).fetch(any())
        assertFalse(handler.checkDigitalAssetLinkRelationship("", "", "", ""))
    }

    private fun mockResponse(body: String): Response {
        val response: Response = mock()
        val mockBody: Response.Body = mock()
        doReturn(200).`when`(response).status
        doReturn(mockBody).`when`(response).body
        doReturn(body).`when`(mockBody).string()
        return response
    }
}
