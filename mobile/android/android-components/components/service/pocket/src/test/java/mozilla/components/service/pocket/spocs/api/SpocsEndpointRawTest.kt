/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.spocs.api

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.service.pocket.helpers.MockResponses
import mozilla.components.service.pocket.helpers.assertClassVisibility
import mozilla.components.service.pocket.helpers.assertRequestParams
import mozilla.components.service.pocket.helpers.assertResponseIsClosed
import mozilla.components.service.pocket.helpers.assertSuccessfulRequestReturnsResponseBody
import mozilla.components.service.pocket.stories.api.PocketEndpointRaw
import mozilla.components.service.pocket.stories.api.PocketEndpointRaw.Companion
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.doThrow
import java.io.IOException
import java.util.UUID
import kotlin.reflect.KVisibility

@RunWith(AndroidJUnit4::class)
class SpocsEndpointRawTest {
    private val profileId = UUID.randomUUID()
    private val appId = "test"

    private lateinit var endpoint: SpocsEndpointRaw
    private lateinit var client: Client

    private lateinit var errorResponse: Response
    private lateinit var successResponse: Response
    private lateinit var defaultResponse: Response

    @Before
    fun setUp() {
        errorResponse = MockResponses.getError()
        successResponse = MockResponses.getSuccess()
        defaultResponse = errorResponse

        client = mock<Client>().also {
            doReturn(defaultResponse).`when`(it).fetch(any())
        }

        endpoint = SpocsEndpointRaw(client, profileId, appId)
    }

    @Test
    fun `GIVEN a PocketEndpointRaw THEN its visibility is internal`() {
        assertClassVisibility(PocketEndpointRaw::class, KVisibility.INTERNAL)
    }

    @Test
    fun `GIVEN a debug build WHEN requesting spocs THEN the appropriate pocket proxy url is used`() {
        SpocsEndpointRaw.isDebugBuild = true
        val expectedUrl = "https://spocs.getpocket.dev/spocs"

        assertRequestParams(
            client,
            makeRequest = {
                endpoint.getSponsoredStories()
            },
            assertParams = { request ->
                assertEquals(expectedUrl, request.url)
                assertEquals(Request.Method.POST, request.method)

                val requestBody = JSONObject(
                    request.body!!.useStream {
                        it.bufferedReader().readText()
                    },
                )
                assertEquals(2, requestBody["version"])
                assertEquals(appId, requestBody["consumer_key"])
                assertEquals(profileId.toString(), requestBody["pocket_id"])

                request.headers!!.first {
                    it.name.equals("Content-Type", true)
                }.value.contains("application/json", true)
            },
        )
    }

    @Test
    fun `GIVEN a release build WHEN requesting spocs THEN the appropriate pocket proxy url is used`() {
        SpocsEndpointRaw.isDebugBuild = false
        val expectedUrl = "https://spocs.getpocket.com/spocs"

        assertRequestParams(
            client,
            makeRequest = {
                endpoint.getSponsoredStories()
            },
            assertParams = { request ->
                assertEquals(expectedUrl, request.url)
                assertEquals(Request.Method.POST, request.method)

                val requestBody = JSONObject(
                    request.body!!.useStream {
                        it.bufferedReader().readText()
                    },
                )
                assertEquals(2, requestBody["version"])
                assertEquals(appId, requestBody["consumer_key"])
                assertEquals(profileId.toString(), requestBody["pocket_id"])

                request.headers!!.first {
                    it.name.equals("Content-Type", true)
                }.value.contains("application/json", true)
            },
        )
    }

    @Test
    fun `WHEN requesting spocs and the client throws an IOException THEN null is returned`() {
        doThrow(IOException::class.java).`when`(client).fetch(any())

        assertNull(endpoint.getSponsoredStories())
    }

    @Test
    fun `WHEN requesting spocs and the response is null THEN null is returned`() {
        doReturn(null).`when`(client).fetch(any())

        assertNull(endpoint.getSponsoredStories())
    }

    @Test
    fun `WHEN requesting spocs and the response is not a success THEN null is returned`() {
        doReturn(errorResponse).`when`(client).fetch(any())

        assertNull(endpoint.getSponsoredStories())
    }

    @Test
    fun `GIVEN a debug build WHEN requesting profile deletion THEN the appropriate pocket proxy url is used`() {
        SpocsEndpointRaw.isDebugBuild = true
        val expectedUrl = "https://spocs.getpocket.dev/user"

        assertRequestParams(
            client,
            makeRequest = {
                endpoint.deleteProfile()
            },
            assertParams = { request ->
                assertEquals(expectedUrl, request.url)
                assertEquals(Request.Method.DELETE, request.method)
            },
        )
    }

    @Test
    fun `GIVEN a release build WHEN requesting profile deletion THEN the appropriate pocket proxy url is used`() {
        SpocsEndpointRaw.isDebugBuild = false
        val expectedUrl = "https://spocs.getpocket.com/user"

        assertRequestParams(
            client,
            makeRequest = {
                endpoint.deleteProfile()
            },
            assertParams = { request ->
                assertEquals(expectedUrl, request.url)
                assertEquals(Request.Method.DELETE, request.method)
            },
        )
    }

    @Test
    fun `WHEN requesting profile deletion and the client throws an IOException THEN false is returned`() {
        doThrow(IOException::class.java).`when`(client).fetch(any())

        assertFalse(endpoint.deleteProfile())
    }

    @Test
    fun `WHEN requesting account deletion and the response is not a success THEN false is returned`() {
        doReturn(errorResponse).`when`(client).fetch(any())

        assertFalse(endpoint.deleteProfile())
    }

    @Test
    fun `WHEN requesting spocs and the response is a success THEN the response body is returned`() {
        assertSuccessfulRequestReturnsResponseBody(client, endpoint::getSponsoredStories)
    }

    @Test
    fun `WHEN requesting profile deletion and the response is a success THEN true is returned`() {
        val response = MockResponses.getSuccess()
        doReturn(response).`when`(client).fetch(any())

        assertTrue(endpoint.deleteProfile())
    }

    @Test
    fun `WHEN requesting spocs and the response is an error THEN response is closed`() {
        assertResponseIsClosed(client, errorResponse) {
            endpoint.getSponsoredStories()
        }
    }

    @Test
    fun `GIVEN a response from the request to delete profile WHEN inferring it's success THEN don't use the reponse body`() {
        // Leverage the fact that a stream can only be read once to know if it was previously read.

        doReturn(errorResponse).`when`(client).fetch(any())
        errorResponse.use { "Only the response status should be used, not the response body" }

        doReturn(successResponse).`when`(client).fetch(any())
        successResponse.use { "Only the response status should be used, not the response body" }
    }

    @Test
    fun `WHEN requesting spocs and the response is a success THEN response is closed`() {
        assertResponseIsClosed(client, successResponse) {
            endpoint.getSponsoredStories()
        }
    }

    @Test
    fun `WHEN newInstance is called THEN a new instance configured with the client provided is returned`() {
        val result = Companion.newInstance(client)

        assertSame(client, result.client)
    }

    @Test
    fun `GIVEN a debug build WHEN querying the base url THEN use the development endpoint`() {
        SpocsEndpointRaw.isDebugBuild = true
        val expectedUrl = "https://spocs.getpocket.dev/"

        assertEquals(expectedUrl, SpocsEndpointRaw.baseUrl)
    }

    @Test
    fun `GIVEN a release build WHEN querying the base url THEN use the production endpoint`() {
        SpocsEndpointRaw.isDebugBuild = false
        val expectedUrl = "https://spocs.getpocket.com/"

        assertEquals(expectedUrl, SpocsEndpointRaw.baseUrl)
    }
}
