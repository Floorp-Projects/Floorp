/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.spocs.api

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.fetch.Client
import mozilla.components.service.pocket.helpers.PocketTestResources
import mozilla.components.service.pocket.helpers.assertClassVisibility
import mozilla.components.service.pocket.helpers.assertResponseIsFailure
import mozilla.components.service.pocket.helpers.assertResponseIsSuccess
import mozilla.components.service.pocket.stories.api.PocketResponse
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.doThrow
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import java.util.UUID
import kotlin.reflect.KVisibility

@OptIn(ExperimentalCoroutinesApi::class) // for runTest
@RunWith(AndroidJUnit4::class)
class SpocsEndpointTest {

    private lateinit var endpoint: SpocsEndpoint
    private var raw: SpocsEndpointRaw = mock() // we shorten the name to avoid confusion with endpoint.
    private var jsonParser: SpocsJSONParser = mock()
    private var client: Client = mock()

    @Before
    fun setUp() {
        endpoint = SpocsEndpoint(raw, jsonParser)
    }

    @Test
    fun `GIVEN a SpocsEndpoint THEN its visibility is internal`() {
        assertClassVisibility(SpocsEndpoint::class, KVisibility.INTERNAL)
    }

    @Test
    fun `GIVEN a request for spocs WHEN getting a null response THEN a failure is returned`() = runTest {
        doReturn(null).`when`(raw).getSponsoredStories()

        assertResponseIsFailure(endpoint.getSponsoredStories())
    }

    @Test
    fun `GIVEN a request for spocs WHEN getting a null response THEN we do not attempt to parse stories`() = runTest {
        doReturn(null).`when`(raw).getSponsoredStories()

        doThrow(
            AssertionError("JSONParser should not be called for a null endpoint response"),
        ).`when`(jsonParser).jsonToSpocs(any())

        endpoint.getSponsoredStories()
    }

    @Test
    fun `GIVEN a request for deleting profile WHEN the response is unsuccessful THEN a failure is returned`() = runTest {
        doReturn(false).`when`(raw).deleteProfile()

        assertResponseIsFailure(endpoint.deleteProfile())
    }

    @Test
    fun `GIVEN a request for deleting profile WHEN the response is successful THEN success is returned`() = runTest {
        doReturn(true).`when`(raw).deleteProfile()

        assertResponseIsSuccess(endpoint.deleteProfile())
    }

    @Test
    fun `GIVEN a request for spocs WHEN getting an empty response THEN a failure is returned`() = runTest {
        arrayOf(
            "",
            " ",
        ).forEach { response ->
            doReturn(response).`when`(raw).getSponsoredStories()

            assertResponseIsFailure(endpoint.getSponsoredStories())
        }
    }

    @Test
    fun `GIVEN a request for spocs WHEN getting an empty response THEN we do not attempt to parse stories`() = runTest {
        arrayOf(
            "",
            " ",
        ).forEach { response ->
            doReturn(response).`when`(raw).getSponsoredStories()
            doThrow(
                AssertionError("JSONParser should not be called for an empty endpoint response"),
            ).`when`(jsonParser).jsonToSpocs(any())

            endpoint.getSponsoredStories()
        }
    }

    @Test
    fun `GIVEN a request for stories WHEN getting a response THEN parse it through PocketJSONParser`() = runTest {
        arrayOf(
            "{}",
            """{"expectedJSON": 101}""",
            """{ "spocs": [] }""",
        ).forEach { response ->
            doReturn(response).`when`(raw).getSponsoredStories()

            endpoint.getSponsoredStories()

            verify(jsonParser, times(1)).jsonToSpocs(response)
        }
    }

    @Test
    fun `GIVEN a request for stories WHEN getting a valid response THEN success is returned`() = runTest {
        endpoint = SpocsEndpoint(raw, SpocsJSONParser)
        val response = PocketTestResources.pocketEndpointThreeSpocsResponse
        doReturn(response).`when`(raw).getSponsoredStories()

        val result = endpoint.getSponsoredStories()

        assertTrue(result is PocketResponse.Success)
    }

    @Test
    fun `GIVEN a request for stories WHEN getting a valid response THEN a success response with parsed stories is returned`() = runTest {
        endpoint = SpocsEndpoint(raw, SpocsJSONParser)
        val response = PocketTestResources.pocketEndpointThreeSpocsResponse
        doReturn(response).`when`(raw).getSponsoredStories()
        val expected = PocketTestResources.apiExpectedPocketSpocs

        val result = endpoint.getSponsoredStories()

        assertEquals(expected, (result as? PocketResponse.Success)?.data)
    }

    @Test
    fun `WHEN newInstance is called THEN a new SpocsEndpoint is returned as a wrapper over a configured SpocsEndpointRaw`() {
        val profileId = UUID.randomUUID()
        val appId = "test"

        val result = SpocsEndpoint.Companion.newInstance(client, profileId, appId)

        assertSame(client, result.rawEndpoint.client)
        assertSame(profileId, result.rawEndpoint.profileId)
        assertSame(appId, result.rawEndpoint.appId)
    }
}
