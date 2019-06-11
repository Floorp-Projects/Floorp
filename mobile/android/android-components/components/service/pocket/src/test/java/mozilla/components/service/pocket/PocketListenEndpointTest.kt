/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.fetch.Client
import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient
import mozilla.components.service.pocket.data.PocketListenArticleMetadata
import mozilla.components.service.pocket.helpers.PocketTestResource
import mozilla.components.service.pocket.helpers.assertResponseIsFailure
import mozilla.components.service.pocket.net.PocketResponse
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Before
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyLong
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.`when`
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

private const val VALID_ACCESS_TOKEN = "accessToken"
private const val VALID_USER_AGENT = "userAgent"

@RunWith(AndroidJUnit4::class)
class PocketListenEndpointTest {

    private lateinit var subject: PocketListenEndpoint
    private lateinit var raw: PocketListenEndpointRaw // we shorten the name to avoid confusion with the endpoint.
    private lateinit var jsonParser: PocketListenJSONParser

    private lateinit var client: Client

    @Before
    fun setUp() {
        raw = mock()
        jsonParser = mock()

        subject = PocketListenEndpoint(raw, jsonParser)

        client = mock()
    }

    @Test
    fun `WHEN getting listen article metadata and the endpoint returns null THEN a failure is returned`() {
        `when`(raw.getArticleListenMetadata(anyLong(), anyString())).thenReturn(null)
        `when`(jsonParser.jsonToListenArticleMetadata(any())).thenThrow(AssertionError("We assume this won't get called so we don't mock it"))
        assertResponseIsFailure(makeRequestGetListenArticleMetadata())
    }

    @Test
    fun `WHEN getting listen article metadata and the endpoint returns a String THEN the String is put into the jsonParser`() {
        arrayOf(
            "",
            " ",
            "{}",
            """{"expectedJSON": 101}"""
        ).forEach { expected ->
            `when`(raw.getArticleListenMetadata(anyLong(), anyString())).thenReturn(expected)
            makeRequestGetListenArticleMetadata()
            verify(jsonParser, times(1)).jsonToListenArticleMetadata(expected)
        }
    }

    @Test
    fun `WHEN getting listen article metadata, the server returns a String, and the JSON parser returns null THEN a failure is returned`() {
        `when`(raw.getArticleListenMetadata(anyLong(), anyString())).thenReturn("")
        `when`(jsonParser.jsonToListenArticleMetadata(any())).thenReturn(null)
        assertResponseIsFailure(makeRequestGetListenArticleMetadata())
    }

    @Test
    fun `WHEN getting listen article metadata, the server returns a String, and the jsonParser returns valid data THEN a success with the data is returned`() {
        val expected = PocketTestResource.getExpectedListenArticleMetadata()
        `when`(raw.getArticleListenMetadata(anyLong(), anyString())).thenReturn("")
        `when`(jsonParser.jsonToListenArticleMetadata(any())).thenReturn(expected)

        val actual = makeRequestGetListenArticleMetadata()
        assertEquals(expected, (actual as? PocketResponse.Success)?.data)
    }

    @Test(expected = IllegalArgumentException::class)
    fun `WHEN newInstance is called with a blank access token key THEN an exception is thrown`() {
        PocketListenEndpoint.newInstance(client, " ", VALID_USER_AGENT)
    }

    @Test(expected = IllegalArgumentException::class)
    fun `WHEN newInstance is called with a blank user agent THEN an exception is thrown`() {
        PocketListenEndpoint.newInstance(client, VALID_ACCESS_TOKEN, " ")
    }

    @Test
    fun `WHEN newInstance is called with valid args THEN no exception is thrown`() {
        PocketListenEndpoint.newInstance(client, VALID_ACCESS_TOKEN, VALID_USER_AGENT)
    }

    @Test
    @Ignore("Requires a network request; run locally to test end-to-end behavior")
    fun `WHEN making an end-to-end listen article metadata request THEN a non-null result is returned`() {
        // To use this test locally, add the access token file (see LISTEN_ACCESS_TOKEN comments) and comment out ignore annotation.
        val client = HttpURLConnectionClient()
        val accessToken = PocketTestResource.LISTEN_ACCESS_TOKEN.get()
        val subject = PocketListenEndpoint.newInstance(client, accessToken, "android-components:PocketListenEndpointTest")

        // The input article may seem invalid but it's test data from the server docs:
        //   https://documenter.getpostman.com/view/777613/S17jXChA#426b37e2-0a4f-bd65-35c6-e14f1b19d0f0
        val response = subject.getListenArticleMetadata(1234, "https://news.com/my-article.html")
        val result = (response as? PocketResponse.Success)?.data

        assertNotNull(result)
        println(result)
    }

    private fun makeRequestGetListenArticleMetadata(): PocketResponse<PocketListenArticleMetadata> {
        return subject.getListenArticleMetadata(42, "https://mozilla.org")
    }
}
