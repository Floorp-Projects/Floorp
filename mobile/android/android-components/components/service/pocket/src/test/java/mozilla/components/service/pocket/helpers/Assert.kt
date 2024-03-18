/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.helpers

import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.service.pocket.stories.api.PocketResponse
import mozilla.components.support.test.any
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.mockito.Mockito.mock
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import kotlin.reflect.KClass
import kotlin.reflect.KVisibility

fun <T : Any> assertConstructorsVisibility(assertedClass: KClass<T>, visibility: KVisibility) {
    assertedClass.constructors.forEach {
        assertEquals(visibility, it.visibility)
    }
}

fun <T : Any> assertClassVisibility(assertedClass: KClass<T>, visibility: KVisibility) {
    assertEquals(visibility, assertedClass.visibility)
}

/**
 * @param client the underlying mock client for the raw endpoint making the request.
 * @param makeRequest makes the request using the raw endpoint.
 * @param assertParams makes assertions on the passed in request.
 */
fun assertRequestParams(client: Client, makeRequest: () -> Unit, assertParams: (Request) -> Unit) {
    whenever(client.fetch(any())).thenAnswer {
        val request = it.arguments[0] as Request
        assertParams(request)
        Response("https://mozilla.org", 200, MutableHeaders(), Response.Body("".byteInputStream()))
    }

    makeRequest()

    // Ensure fetch is called so that the assertions in assertParams are called.
    verify(client, times(1)).fetch(any())
}

/**
 * @param client the underlying mock client for the raw endpoint making the request.
 * @param makeRequest makes the request using the raw endpoint and returns the body text, or null on error
 */
fun assertSuccessfulRequestReturnsResponseBody(client: Client, makeRequest: () -> String?) {
    val expectedBody = "{\"jsonStr\": true}"
    val body = mock(Response.Body::class.java).also {
        whenever(it.string()).thenReturn(expectedBody)
    }
    val response = MockResponses.getSuccess().also {
        whenever(it.body).thenReturn(body)
    }
    whenever(client.fetch(any())).thenReturn(response)

    assertEquals(expectedBody, makeRequest())
}

/**
 * @param client the underlying mock client for the raw endpoint making the request.
 * @param response the response to return when the request is made.
 * @param makeRequest makes the request using the raw endpoint.
 */
fun assertResponseIsClosed(client: Client, response: Response, makeRequest: () -> Unit) {
    whenever(client.fetch(any())).thenReturn(response)
    makeRequest()
    verify(response, times(1)).close()
}

fun assertResponseIsFailure(response: Any) {
    assertEquals(PocketResponse.Failure::class.java, response.javaClass)
}

fun assertResponseIsSuccess(response: Any) {
    assertEquals(PocketResponse.Success::class.java, response.javaClass)
}
