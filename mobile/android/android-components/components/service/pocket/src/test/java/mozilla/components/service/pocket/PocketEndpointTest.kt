/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.fetch.Client
import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient
import mozilla.components.service.pocket.helpers.PocketTestResource
import mozilla.components.service.pocket.helpers.assertResponseIsFailure
import mozilla.components.service.pocket.net.PocketResponse
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

private const val VALID_API_KEY = "apiKey"
private const val VALID_USER_AGENT = "userAgent"

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
    fun `WHEN getting video recommendations and the endpoint returns null THEN a failure is returned`() {
        `when`(raw.getGlobalVideoRecommendations()).thenReturn(null)
        `when`(jsonParser.jsonToGlobalVideoRecommendations(any())).thenThrow(AssertionError("We assume this won't get called so we don't mock it"))
        assertResponseIsFailure(endpoint.getGlobalVideoRecommendations())
    }

    @Test
    fun `WHEN getting video recommendations and the endpoint returns a String THEN the String is put into the jsonParser`() {
        arrayOf(
            "",
            " ",
            "{}",
            """{"expectedJSON": 101}"""
        ).forEach { expected ->
            `when`(raw.getGlobalVideoRecommendations()).thenReturn(expected)
            endpoint.getGlobalVideoRecommendations()
            verify(jsonParser, times(1)).jsonToGlobalVideoRecommendations(expected)
        }
    }

    @Test
    fun `WHEN getting video recommendations, the server returns a String, and the JSON parser returns null THEN a failure is returned`() {
        `when`(raw.getGlobalVideoRecommendations()).thenReturn("")
        `when`(jsonParser.jsonToGlobalVideoRecommendations(any())).thenReturn(null)
        assertResponseIsFailure(endpoint.getGlobalVideoRecommendations())
    }

    @Test
    fun `WHEN getting video recommendations, the server returns a String, and the jsonParser returns an empty list THEN a failure is returned`() {
        `when`(raw.getGlobalVideoRecommendations()).thenReturn("")
        `when`(jsonParser.jsonToGlobalVideoRecommendations(any())).thenReturn(emptyList())
        assertResponseIsFailure(endpoint.getGlobalVideoRecommendations())
    }

    @Test
    fun `WHEN getting video recommendations, the server returns a String, and the jsonParser returns valid data THEN a success with the data is returned`() {
        val expected = PocketTestResource.getExpectedPocketVideoRecommendationFirstTwo()
        `when`(raw.getGlobalVideoRecommendations()).thenReturn("")
        `when`(jsonParser.jsonToGlobalVideoRecommendations(any())).thenReturn(expected)

        val actual = endpoint.getGlobalVideoRecommendations()
        assertEquals(expected, (actual as? PocketResponse.Success)?.data)
    }

    @Test(expected = IllegalArgumentException::class)
    fun `WHEN newInstance is called with a blank API key THEN an exception is thrown`() {
        PocketEndpoint.newInstance(client, " ", VALID_USER_AGENT)
    }

    @Test(expected = IllegalArgumentException::class)
    fun `WHEN newInstance is called with a blank user agent THEN an exception is thrown`() {
        PocketEndpoint.newInstance(client, VALID_API_KEY, " ")
    }

    @Test
    fun `WHEN newInstance is called with valid args THEN no exception is thrown`() {
        PocketEndpoint.newInstance(client, VALID_API_KEY, VALID_USER_AGENT)
    }

    @Test
    @Ignore("Requires a network request; run locally to test end-to-end behavior")
    fun `WHEN making an end-to-end global video recommendations request THEN 20 results are returned`() {
        // To use this test locally, add the API key file (see API_KEY comments) and comment out ignore annotation.
        val client = HttpURLConnectionClient()
        val apiKey = PocketTestResource.API_KEY.get()
        val endpoint = PocketEndpoint.newInstance(client, apiKey, "android-components:PocketEndpointTest")
        val response = endpoint.getGlobalVideoRecommendations()
        val results = (response as? PocketResponse.Success)?.data

        assertEquals(20, results?.size)
        results?.forEach { println(it.toString()) }
    }
}
