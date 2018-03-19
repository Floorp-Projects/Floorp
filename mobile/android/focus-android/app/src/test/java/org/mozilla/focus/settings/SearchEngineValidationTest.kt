/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings

import okhttp3.mockwebserver.MockResponse
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

import okhttp3.mockwebserver.MockWebServer
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.mozilla.focus.settings.ManualAddSearchEngineSettingsFragment.Companion.isValidSearchQueryURL

@RunWith(RobolectricTestRunner::class)
// This unit test is not running on an Android device. Allow me to use spaces in function names.
@Suppress("IllegalIdentifier")
class SearchEngineValidationTest {
    @Test
    fun `URL returning 200 OK is valid`() = withMockWebServer(responseWithStatus(200)) {
        assertTrue(isValidSearchQueryURL(it.rootUrl()))
    }

    @Test
    fun `URL using HTTP redirect is invalid`() = withMockWebServer(responseWithStatus(301)) {
        // We now follow redirects(Issue #1976). This test now asserts false.
        assertFalse(isValidSearchQueryURL(it.rootUrl()))
    }

    @Test
    fun `URL returning 404 NOT FOUND is not valid`() = withMockWebServer(responseWithStatus(404)) {
        assertFalse(isValidSearchQueryURL(it.rootUrl()))
    }

    @Test
    fun `URL returning server error is not valid`() = withMockWebServer(responseWithStatus(500)) {
        assertFalse(isValidSearchQueryURL(it.rootUrl()))
    }

    @Test
    fun `URL timing out is not valid`() = withMockWebServer {
        // Without queuing a response MockWebServer will not return anything and keep the connection open
        assertFalse(isValidSearchQueryURL(it.rootUrl()))
    }
}

/**
 * Helper for creating a test that uses a mock webserver instance.
 */
private fun withMockWebServer(vararg responses: MockResponse, block: (MockWebServer) -> Unit) {
    val server = MockWebServer()

    responses.forEach { server.enqueue(it) }

    server.start()

    try {
        block(server)
    } finally {
        server.shutdown()
    }
}

private fun MockWebServer.rootUrl(): String = url("/").toString()

private fun responseWithStatus(status: Int) =
        MockResponse()
                .setResponseCode(status)
                .setBody("")