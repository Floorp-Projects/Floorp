/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.api

import androidx.core.net.toUri
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Response
import mozilla.components.service.pocket.helpers.MockResponses
import mozilla.components.service.pocket.helpers.TEST_STORIES_COUNT
import mozilla.components.service.pocket.helpers.TEST_STORIES_LOCALE
import mozilla.components.service.pocket.helpers.TEST_URL
import mozilla.components.service.pocket.helpers.TEST_VALID_API_KEY
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

    private lateinit var endpoint: PocketEndpointRaw
    private lateinit var client: Client
    private lateinit var urls: PocketURLs

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

        urls = mock<PocketURLs>().also {
            whenever(it.getLocaleStoriesRecommendations(TEST_STORIES_COUNT, TEST_STORIES_LOCALE)).thenReturn(TEST_URL)
        }
        endpoint = PocketEndpointRaw(client, urls)
    }

    @Test
    fun `GIVEN a PocketEndpointRaw THEN its visibility is internal`() {
        assertClassVisibility(PocketEndpointRaw::class, KVisibility.INTERNAL)
    }

    @Test
    fun `WHEN requesting global stories recs THEN the global stories recs url is used`() {
        val expectedUrl = "https://mozilla.org/global-recs"
        whenever(urls.getLocaleStoriesRecommendations(TEST_STORIES_COUNT, TEST_STORIES_LOCALE)).thenReturn(expectedUrl.toUri())

        assertRequestParams(client, makeRequest = {
            endpoint.getGlobalStoriesRecommendations(TEST_STORIES_COUNT, TEST_STORIES_LOCALE)
        }, assertParams = { request ->
            assertEquals(expectedUrl, request.url)
        })
    }

    @Test
    fun `WHEN requesting global stories recs and the client throws an IOException THEN null is returned`() {
        whenever(client.fetch(any())).thenThrow(IOException::class.java)
        assertNull(endpoint.getGlobalStoriesRecommendations(TEST_STORIES_COUNT, TEST_STORIES_LOCALE))
    }

    @Test
    fun `WHEN requesting global stories recs and the response is null THEN null is returned`() {
        whenever(client.fetch(any())).thenReturn(null)
        assertNull(endpoint.getGlobalStoriesRecommendations(TEST_STORIES_COUNT, TEST_STORIES_LOCALE))
    }

    @Test
    fun `WHEN requesting global stories recs and the response is not a success THEN null is returned`() {
        whenever(client.fetch(any())).thenReturn(errorResponse)
        assertNull(endpoint.getGlobalStoriesRecommendations(TEST_STORIES_COUNT, TEST_STORIES_LOCALE))
    }

    @Test
    fun `WHEN requesting global stories recs and the response is a success THEN the response body is returned`() {
        assertSuccessfulRequestReturnsResponseBody(client, endpoint::getGlobalStoriesRecommendations)
    }

    @Test
    fun `WHEN requesting global stories recs and the response is an error THEN response is closed`() {
        assertResponseIsClosed(client, errorResponse) {
            endpoint.getGlobalStoriesRecommendations(TEST_STORIES_COUNT, TEST_STORIES_LOCALE)
        }
    }

    @Test
    fun `WHEN requesting global stories recs and the response is a success THEN response is closed`() {
        assertResponseIsClosed(client, successResponse) {
            endpoint.getGlobalStoriesRecommendations(TEST_STORIES_COUNT, TEST_STORIES_LOCALE)
        }
    }

    @Test(expected = IllegalArgumentException::class)
    fun `WHEN newInstance is called with a blank API key THEN an exception is thrown`() {
        PocketEndpointRaw.newInstance(" ", client)
    }

    @Test
    fun `WHEN newInstance is called with valid args THEN no exception is thrown`() {
        PocketEndpointRaw.newInstance(TEST_VALID_API_KEY, client)
    }

    @Test
    fun `WHEN newInstance is called THEN a new instance configured with the client and api key provided is returned`() {
        val result = PocketEndpointRaw.newInstance(TEST_VALID_API_KEY, client)

        assertSame(TEST_VALID_API_KEY, result.urls.apiKey)
        assertSame(client, result.client)
    }
}
