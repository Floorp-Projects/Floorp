/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.stories.api

import androidx.core.net.toUri
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Response
import mozilla.components.service.pocket.helpers.MockResponses
import mozilla.components.service.pocket.helpers.assertClassVisibility
import mozilla.components.service.pocket.helpers.assertRequestParams
import mozilla.components.service.pocket.helpers.assertResponseIsClosed
import mozilla.components.service.pocket.helpers.assertSuccessfulRequestReturnsResponseBody
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import java.io.IOException
import kotlin.reflect.KVisibility

@RunWith(AndroidJUnit4::class)
class PocketEndpointRawTest {
    private val url = "https://mozilla.org".toUri()

    private lateinit var endpoint: PocketEndpointRaw
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
            whenever(it.fetch(any())).thenReturn(defaultResponse)
        }

        endpoint = PocketEndpointRaw(client)
    }

    @Test
    fun `GIVEN a PocketEndpointRaw THEN its visibility is internal`() {
        assertClassVisibility(PocketEndpointRaw::class, KVisibility.INTERNAL)
    }

    @Test
    fun `WHEN requesting stories recommendations THEN the firefox android home recommendations url is used`() {
        val expectedUrl = "https://firefox-android-home-recommendations.getpocket.com/"

        assertRequestParams(
            client,
            makeRequest = {
                endpoint.getRecommendedStories()
            },
            assertParams = { request ->
                assertEquals(expectedUrl, request.url)
            },
        )
    }

    @Test
    fun `WHEN requesting stories recommendations and the client throws an IOException THEN null is returned`() {
        whenever(client.fetch(any())).thenThrow(IOException::class.java)
        assertNull(endpoint.getRecommendedStories())
    }

    @Test
    fun `WHEN requesting stories recommendations and the response is null THEN null is returned`() {
        whenever(client.fetch(any())).thenReturn(null)
        assertNull(endpoint.getRecommendedStories())
    }

    @Test
    fun `WHEN requesting stories recommendations and the response is not a success THEN null is returned`() {
        whenever(client.fetch(any())).thenReturn(errorResponse)
        assertNull(endpoint.getRecommendedStories())
    }

    @Test
    fun `WHEN requesting stories recommendations and the response is a success THEN the response body is returned`() {
        assertSuccessfulRequestReturnsResponseBody(client, endpoint::getRecommendedStories)
    }

    @Test
    fun `WHEN requesting stories recommendations and the response is an error THEN response is closed`() {
        assertResponseIsClosed(client, errorResponse) {
            endpoint.getRecommendedStories()
        }
    }

    @Test
    fun `WHEN requesting stories recommendations and the response is a success THEN response is closed`() {
        assertResponseIsClosed(client, successResponse) {
            endpoint.getRecommendedStories()
        }
    }

    @Test
    fun `WHEN newInstance is called THEN a new instance configured with the client provided is returned`() {
        val result = PocketEndpointRaw.newInstance(client)

        assertSame(client, result.client)
    }
}
