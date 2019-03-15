/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.request

import mozilla.components.browser.errorpages.ErrorType
import mozilla.components.concept.engine.EngineSession
import org.junit.Test
import mozilla.components.concept.engine.request.RequestInterceptor.InterceptionResponse
import mozilla.components.concept.engine.request.RequestInterceptor.ErrorResponse
import org.junit.Assert.assertEquals
import org.mockito.Mockito.mock

class RequestInterceptorTest {

    @Test
    fun `match interception response`() {
        val urlResponse = InterceptionResponse.Url("https://mozilla.org")
        val contentReponse = InterceptionResponse.Content("data")

        val url: String = urlResponse.url

        val content: Triple<String, String, String> =
            Triple(contentReponse.data, contentReponse.encoding, contentReponse.mimeType)

        assertEquals("https://mozilla.org", url)
        assertEquals(Triple("data", "UTF-8", "text/html"), content)
    }

    @Test
    fun `error response has default values`() {
        val errorResponse = ErrorResponse("data")
        assertEquals("data", errorResponse.data)
        assertEquals("text/html", errorResponse.mimeType)
        assertEquals("UTF-8", errorResponse.encoding)
        assertEquals(null, errorResponse.url)
    }

    @Test
    fun `interceptor has default methods`() {
        val engineSession = mock(EngineSession::class.java)
        val interceptor = object : RequestInterceptor { }
        interceptor.onLoadRequest(engineSession, "url")
        interceptor.onErrorRequest(engineSession, ErrorType.ERROR_UNKNOWN_SOCKET_TYPE, null)
    }
}