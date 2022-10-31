/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.ext

import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import java.io.IOException

private const val EXPECTED_DEFAULT_RESPONSE_BODY = "default response body"
private const val TEST_URL = "https://mozilla.org"

class ConceptFetchKtTest {

    private lateinit var client: Client
    private lateinit var defaultResponse: Response
    private lateinit var failureResponse: Response
    private lateinit var testRequest: Request

    @Before
    fun setUp() {
        val responseBody = Response.Body(EXPECTED_DEFAULT_RESPONSE_BODY.byteInputStream())
        val failureResponseBody = Response.Body("failure response body)".byteInputStream())
        defaultResponse = spy(Response(TEST_URL, 200, MutableHeaders(), responseBody))
        failureResponse = spy(Response(TEST_URL, 404, MutableHeaders(), failureResponseBody))
        testRequest = Request(TEST_URL)

        client = mock<Client>().also {
            whenever(it.fetch(any())).thenReturn(defaultResponse)
        }
    }

    @Test
    fun `GIVEN fetch throws an exception WHEN fetchBodyOrNull is called THEN null is returned`() {
        whenever(client.fetch(any())).thenThrow(IOException())
        assertNull(client.fetchBodyOrNull(testRequest))
    }

    @Test
    fun `GIVEN fetch returns a failure response WHEN fetchBodyOrNull is called THEN null is returned`() {
        setUpClientFailureResponse()
        assertNull(client.fetchBodyOrNull(testRequest))
    }

    @Test
    fun `GIVEN fetch returns a success response WHEN fetchBodyOrNull is called THEN the response body is returned`() {
        val actual = client.fetchBodyOrNull(testRequest)
        assertEquals(EXPECTED_DEFAULT_RESPONSE_BODY, actual)
    }

    @Test
    fun `GIVEN fetch returns a success response WHEN fetchBodyOrNull is called THEN the response is closed`() {
        client.fetchBodyOrNull(testRequest)
        verify(defaultResponse, times(1)).close()
    }

    @Test
    fun `GIVEN fetch returns a failure response WHEN fetchBodyOrNull is called THEN the response is closed`() {
        setUpClientFailureResponse()
        client.fetchBodyOrNull(testRequest)
        verify(failureResponse, times(1)).close()
    }

    private fun setUpClientFailureResponse() {
        whenever(client.fetch(any())).thenReturn(failureResponse)
    }
}
