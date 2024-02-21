/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import io.mockk.mockk
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags.Companion.ALLOW_ADDITIONAL_HEADERS
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags.Companion.LOAD_FLAGS_BYPASS_LOAD_URI_DELEGATE
import mozilla.components.concept.engine.request.RequestInterceptor
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class UrlRequestInterceptorTest {

    private lateinit var engineSession: EngineSession

    @Before
    fun setup() {
        engineSession = mockk(relaxed = true)
    }

    @Test
    fun `GIVEN device is above threshold WHEN get additional headers is called THEN return the correct map of additional headers`() {
        val isDeviceRamAboveThreshold = true
        val urlRequestInterceptor = getUrlRequestInterceptor(
            isDeviceRamAboveThreshold = isDeviceRamAboveThreshold,
        )

        assertEquals(
            mapOf("X-Search-Subdivision" to "1"),
            urlRequestInterceptor.getAdditionalHeaders(isDeviceRamAboveThreshold),
        )
    }

    @Test
    fun `GIVEN device is not above threshold WHEN get additional headers is called THEN return the correct map of additional headers`() {
        val isDeviceRamAboveThreshold = false
        val urlRequestInterceptor = getUrlRequestInterceptor(
            isDeviceRamAboveThreshold = isDeviceRamAboveThreshold,
        )

        assertEquals(
            mapOf("X-Search-Subdivision" to "0"),
            urlRequestInterceptor.getAdditionalHeaders(isDeviceRamAboveThreshold),
        )
    }

    @Test
    fun `WHEN should intercept request is called THEN return the correct boolean value`() {
        val urlRequestInterceptor = getUrlRequestInterceptor()

        assertTrue(
            urlRequestInterceptor.shouldInterceptRequest(
                uri = "https://www.google.com",
                isSubframeRequest = false,
            ),
        )
        assertTrue(
            urlRequestInterceptor.shouldInterceptRequest(
                uri = "https://www.google.com/webhp",
                isSubframeRequest = false,
            ),
        )
        assertTrue(
            urlRequestInterceptor.shouldInterceptRequest(
                uri = "https://www.google.com/preferences",
                isSubframeRequest = false,
            ),
        )
        assertTrue(
            urlRequestInterceptor.shouldInterceptRequest(
                uri = "https://www.google.com/search?q=blue",
                isSubframeRequest = false,
            ),
        )
        assertTrue(
            urlRequestInterceptor.shouldInterceptRequest(
                uri = "https://www.google.ca/search?q=red",
                isSubframeRequest = false,
            ),
        )
        assertTrue(
            urlRequestInterceptor.shouldInterceptRequest(
                uri = "https://www.google.co.jp/search?q=red",
                isSubframeRequest = false,
            ),
        )

        assertFalse(
            urlRequestInterceptor.shouldInterceptRequest(
                uri = "https://getpocket.com",
                isSubframeRequest = false,
            ),
        )
        assertFalse(
            urlRequestInterceptor.shouldInterceptRequest(
                uri = "https://www.google.com/search?q=blue",
                isSubframeRequest = true,
            ),
        )
        assertFalse(
            urlRequestInterceptor.shouldInterceptRequest(
                uri = "https://www.google.com/recaptcha",
                isSubframeRequest = true,
            ),
        )
    }

    @Test
    fun `WHEN a Pocket request is loaded THEN request is not intercepted`() {
        val uri = "https://getpocket.com"
        val response = getUrlRequestInterceptor().onLoadRequest(
            uri = uri,
        )

        assertNull(response)
    }

    @Test
    fun `WHEN a Google request is loaded THEN request is intercepted`() {
        val uri = "https://www.google.com"

        assertEquals(
            RequestInterceptor.InterceptionResponse.Url(
                url = uri,
                flags = LoadUrlFlags.select(
                    LOAD_FLAGS_BYPASS_LOAD_URI_DELEGATE,
                    ALLOW_ADDITIONAL_HEADERS,
                ),
                additionalHeaders = mapOf(
                    "X-Search-Subdivision" to "0",
                ),
            ),
            getUrlRequestInterceptor().onLoadRequest(
                uri = uri,
            ),
        )
    }

    @Test
    fun `WHEN a Google search request is loaded THEN request is intercepted`() {
        val uri = "https://www.google.com/search?q=blue"

        assertEquals(
            RequestInterceptor.InterceptionResponse.Url(
                url = uri,
                flags = LoadUrlFlags.select(
                    LOAD_FLAGS_BYPASS_LOAD_URI_DELEGATE,
                    ALLOW_ADDITIONAL_HEADERS,
                ),
                additionalHeaders = mapOf(
                    "X-Search-Subdivision" to "1",
                ),
            ),
            getUrlRequestInterceptor(isDeviceRamAboveThreshold = true).onLoadRequest(
                uri = uri,
            ),
        )

        assertEquals(
            RequestInterceptor.InterceptionResponse.Url(
                url = uri,
                flags = LoadUrlFlags.select(
                    LOAD_FLAGS_BYPASS_LOAD_URI_DELEGATE,
                    ALLOW_ADDITIONAL_HEADERS,
                ),
                additionalHeaders = mapOf(
                    "X-Search-Subdivision" to "0",
                ),
            ),
            getUrlRequestInterceptor(isDeviceRamAboveThreshold = false).onLoadRequest(
                uri = uri,
            ),
        )
    }

    @Test
    fun `WHEN a Google search request with a ca TLD request is loaded THEN request is intercepted`() {
        val uri = "https://www.google.ca/search?q=red"

        assertEquals(
            RequestInterceptor.InterceptionResponse.Url(
                url = uri,
                flags = LoadUrlFlags.select(
                    LOAD_FLAGS_BYPASS_LOAD_URI_DELEGATE,
                    ALLOW_ADDITIONAL_HEADERS,
                ),
                additionalHeaders = mapOf(
                    "X-Search-Subdivision" to "1",
                ),
            ),
            getUrlRequestInterceptor(isDeviceRamAboveThreshold = true).onLoadRequest(
                uri = uri,
            ),
        )

        assertEquals(
            RequestInterceptor.InterceptionResponse.Url(
                url = uri,
                flags = LoadUrlFlags.select(
                    LOAD_FLAGS_BYPASS_LOAD_URI_DELEGATE,
                    ALLOW_ADDITIONAL_HEADERS,
                ),
                additionalHeaders = mapOf(
                    "X-Search-Subdivision" to "0",
                ),
            ),
            getUrlRequestInterceptor(isDeviceRamAboveThreshold = false).onLoadRequest(
                uri = uri,
            ),
        )
    }

    @Test
    fun `WHEN a Google subframe request is loaded THEN request is not intercepted`() {
        val uri = "https://www.google.com/search?q=blue"

        assertNull(
            getUrlRequestInterceptor(isDeviceRamAboveThreshold = true).onLoadRequest(
                uri = uri,
                isSubframeRequest = true,
            ),
        )
    }

    private fun getUrlRequestInterceptor(isDeviceRamAboveThreshold: Boolean = false) =
        UrlRequestInterceptor(
            isDeviceRamAboveThreshold = isDeviceRamAboveThreshold,
        )

    private fun UrlRequestInterceptor.onLoadRequest(
        uri: String,
        lastUri: String? = null,
        hasUserGesture: Boolean = false,
        isSameDomain: Boolean = false,
        isRedirect: Boolean = false,
        isDirectNavigation: Boolean = false,
        isSubframeRequest: Boolean = false,
    ) = this.onLoadRequest(
        engineSession = engineSession,
        uri = uri,
        lastUri = lastUri,
        hasUserGesture = hasUserGesture,
        isSameDomain = isSameDomain,
        isRedirect = isRedirect,
        isDirectNavigation = isDirectNavigation,
        isSubframeRequest = isSubframeRequest,
    )
}
