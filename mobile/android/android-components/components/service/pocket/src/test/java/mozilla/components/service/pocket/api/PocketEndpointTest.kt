/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.api

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.fetch.Client
import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient
import mozilla.components.service.pocket.helpers.PocketTestResource
import mozilla.components.service.pocket.helpers.TEST_STORIES_COUNT
import mozilla.components.service.pocket.helpers.TEST_STORIES_LOCALE
import mozilla.components.service.pocket.helpers.TEST_VALID_API_KEY
import mozilla.components.service.pocket.helpers.assertClassVisibility
import mozilla.components.service.pocket.helpers.assertResponseIsFailure
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertSame
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.reflect.KVisibility

@RunWith(AndroidJUnit4::class)
class PocketEndpointTest {

    private lateinit var endpoint: PocketEndpoint
    private lateinit var raw: PocketEndpointRaw // we shorten the name to avoid confusion with endpoint.

    private lateinit var client: Client

    @Before
    fun setUp() {
        raw = mock()
        endpoint = PocketEndpoint(raw)

        client = mock()
    }

    @Test
    fun `GIVEN a PocketEndpoint THEN its visibility is internal`() {
        assertClassVisibility(PocketEndpoint::class, KVisibility.INTERNAL)
    }

    @Test
    fun `WHEN getting stories recommendations and the endpoint returns null THEN a failure is returned`() {
        whenever(raw.getGlobalStoriesRecommendations(TEST_STORIES_COUNT, TEST_STORIES_LOCALE)).thenReturn(null)
        assertResponseIsFailure(endpoint.getTopStories(TEST_STORIES_COUNT, TEST_STORIES_LOCALE))
    }

    @Test
    fun `WHEN getting stories recommendations and the endpoint returns an empty response THEN a failure is returned`() {
        whenever(raw.getGlobalStoriesRecommendations(TEST_STORIES_COUNT, TEST_STORIES_LOCALE)).thenReturn("")
        assertResponseIsFailure(endpoint.getTopStories(TEST_STORIES_COUNT, TEST_STORIES_LOCALE))
    }

    @Test
    fun `WHEN the Pocket API returns a String as a response THEN a success with the data is returned`() {
        val expected = PocketTestResource.FIVE_POCKET_STORIES_RECOMMENDATIONS_POCKET_RESPONSE.get()
        whenever(raw.getGlobalStoriesRecommendations(TEST_STORIES_COUNT, TEST_STORIES_LOCALE)).thenReturn(expected)

        val actual = endpoint.getTopStories(TEST_STORIES_COUNT, TEST_STORIES_LOCALE)
        assertEquals(expected, (actual as? PocketResponse.Success)?.data)
    }

    @Test
    fun `WHEN newInstance is called THEN a new PocketEndpoint is returned as a wrapper over a configured PocketEndpointRaw`() {
        val client: HttpURLConnectionClient = mock()

        val result = PocketEndpoint.newInstance(TEST_VALID_API_KEY, client)

        assertSame(TEST_VALID_API_KEY, result.rawEndpoint.urls.apiKey)
        assertSame(client, result.rawEndpoint.client)
    }

    @Test(expected = IllegalArgumentException::class)
    fun `WHEN newInstance is called with a blank API key THEN an exception is thrown`() {
        PocketEndpoint.newInstance(" ", client)
    }

    @Test
    fun `WHEN newInstance is called with valid args THEN no exception is thrown`() {
        PocketEndpoint.newInstance(TEST_VALID_API_KEY, client)
    }
}
