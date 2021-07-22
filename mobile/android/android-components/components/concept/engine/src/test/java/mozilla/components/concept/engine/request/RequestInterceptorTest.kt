/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.request

import mozilla.components.browser.errorpages.ErrorType
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.request.RequestInterceptor.InterceptionResponse
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mockito.Mockito.mock

class RequestInterceptorTest {

    @Test
    fun `match interception response`() {
        val urlResponse = InterceptionResponse.Url("https://mozilla.org")
        val contentResponse = InterceptionResponse.Content("data")

        val url: String = urlResponse.url

        val content: Triple<String, String, String> =
            Triple(contentResponse.data, contentResponse.encoding, contentResponse.mimeType)

        assertEquals("https://mozilla.org", url)
        assertEquals(Triple("data", "UTF-8", "text/html"), content)
    }

    @Test
    fun `interceptor has default methods`() {
        val engineSession = mock(EngineSession::class.java)
        val interceptor = object : RequestInterceptor { }
        interceptor.onLoadRequest(engineSession, "url", null, false, false, false, false, false)
        interceptor.onErrorRequest(engineSession, ErrorType.ERROR_UNKNOWN_SOCKET_TYPE, null)
    }
}
