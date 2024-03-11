/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import androidx.annotation.VisibleForTesting
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags.Companion.ALLOW_ADDITIONAL_HEADERS
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags.Companion.BYPASS_CACHE
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags.Companion.LOAD_FLAGS_BYPASS_LOAD_URI_DELEGATE
import mozilla.components.concept.engine.request.RequestInterceptor
import java.net.MalformedURLException
import java.net.URL

/**
 * [RequestInterceptor] implementation for intercepting URL load requests to allow custom
 * behaviour.
 *
 * @param isDeviceRamAboveThreshold Whether or not the device ram is above a threshold.
 */
class UrlRequestInterceptor(private val isDeviceRamAboveThreshold: Boolean) : RequestInterceptor {

    private val isGoogleRequest by lazy {
        Regex("^https://www\\.google\\..+")
    }
    private val googleRequestPaths = setOf("/search", "/webhp", "/preferences")

    @VisibleForTesting
    internal fun getAdditionalHeaders(isDeviceRamAboveThreshold: Boolean): Map<String, String> {
        val value = if (isDeviceRamAboveThreshold) {
            "1"
        } else {
            "0"
        }

        return mapOf(
            "X-Search-Subdivision" to value,
        )
    }

    @VisibleForTesting
    internal fun shouldInterceptRequest(
        uri: String,
        isSubframeRequest: Boolean,
    ): Boolean {
        if (isSubframeRequest || !isGoogleRequest.containsMatchIn(uri)) {
            return false
        }

        return try {
            googleRequestPaths.contains(URL(uri).path)
        } catch (e: MalformedURLException) {
            false
        }
    }

    override fun onLoadRequest(
        engineSession: EngineSession,
        uri: String,
        lastUri: String?,
        hasUserGesture: Boolean,
        isSameDomain: Boolean,
        isRedirect: Boolean,
        isDirectNavigation: Boolean,
        isSubframeRequest: Boolean,
    ): RequestInterceptor.InterceptionResponse? {
        if (!shouldInterceptRequest(uri = uri, isSubframeRequest = isSubframeRequest)) {
            return null
        }

        // This is a workaround for https://bugzilla.mozilla.org/show_bug.cgi?id=1875668
        // Remove this by implementing https://bugzilla.mozilla.org/show_bug.cgi?id=1883496
        val loadUrlFlags = if (uri.endsWith("#ip=1", true)) {
            LoadUrlFlags.select(
                LOAD_FLAGS_BYPASS_LOAD_URI_DELEGATE,
                ALLOW_ADDITIONAL_HEADERS,
                BYPASS_CACHE,
            )
        } else {
            LoadUrlFlags.select(
                LOAD_FLAGS_BYPASS_LOAD_URI_DELEGATE,
                ALLOW_ADDITIONAL_HEADERS,
            )
        }

        return RequestInterceptor.InterceptionResponse.Url(
            url = uri,
            flags = loadUrlFlags,
            additionalHeaders = getAdditionalHeaders(isDeviceRamAboveThreshold),
        )
    }
}
