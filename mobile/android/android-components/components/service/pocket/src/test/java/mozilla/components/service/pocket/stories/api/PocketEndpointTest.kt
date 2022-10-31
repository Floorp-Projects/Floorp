/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.stories.api

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.fetch.Client
import mozilla.components.service.pocket.helpers.PocketTestResources
import mozilla.components.service.pocket.helpers.assertClassVisibility
import mozilla.components.service.pocket.helpers.assertResponseIsFailure
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertSame
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import kotlin.reflect.KVisibility

@RunWith(AndroidJUnit4::class)
class PocketEndpointTest {

    private lateinit var endpoint: PocketEndpoint
    private lateinit var raw: PocketEndpointRaw // we shorten the name to avoid confusion with endpoint.
    private lateinit var jsonParser: PocketJSONParser

    private lateinit var client: Client

    @Before
    fun setUp() {
        raw = mock()
        jsonParser = mock()
        endpoint = PocketEndpoint(raw, jsonParser)

        client = mock()
    }

    @Test
    fun `GIVEN a PocketEndpoint THEN its visibility is internal`() {
        assertClassVisibility(PocketEndpoint::class, KVisibility.INTERNAL)
    }

    @Test
    fun `GIVEN an api request for stories WHEN getting a null response THEN a failure is returned`() {
        whenever(raw.getRecommendedStories()).thenReturn(null)
        whenever(jsonParser.jsonToPocketApiStories(any())).thenThrow(
            AssertionError(
                "We assume this won't get called so we don't mock it",
            ),
        )

        assertResponseIsFailure(endpoint.getRecommendedStories())
    }

    @Test
    fun `GIVEN an api request for stories WHEN getting an empty response THEN a failure is returned`() {
        whenever(raw.getRecommendedStories()).thenReturn("")
        whenever(jsonParser.jsonToPocketApiStories(any())).thenReturn(null)

        assertResponseIsFailure(endpoint.getRecommendedStories())
    }

    @Test
    fun `GIVEN an api request for stories WHEN getting a response THEN parse map it through PocketJSONParser`() {
        arrayOf(
            "",
            " ",
            "{}",
            """{"expectedJSON": 101}""",
        ).forEach { expected ->
            whenever(raw.getRecommendedStories()).thenReturn(expected)

            endpoint.getRecommendedStories()

            verify(jsonParser, times(1)).jsonToPocketApiStories(expected)
        }
    }

    @Test
    fun `GIVEN an api request for stories WHEN getting a valid response THEN a success with the data is returned`() {
        val expected = PocketTestResources.apiExpectedPocketStoriesRecommendations
        whenever(raw.getRecommendedStories()).thenReturn("")
        whenever(jsonParser.jsonToPocketApiStories(any())).thenReturn(expected)

        val actual = endpoint.getRecommendedStories()

        assertEquals(expected, (actual as? PocketResponse.Success)?.data)
    }

    @Test
    fun `WHEN newInstance is called THEN a new PocketEndpoint is returned as a wrapper over a configured PocketEndpointRaw`() {
        val result = PocketEndpoint.newInstance(client)

        assertSame(client, result.rawEndpoint.client)
    }
}
