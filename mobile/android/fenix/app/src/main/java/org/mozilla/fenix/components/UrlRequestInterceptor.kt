/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import androidx.annotation.VisibleForTesting
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags.Companion.ALLOW_ADDITIONAL_HEADERS
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags.Companion.LOAD_FLAGS_BYPASS_LOAD_URI_DELEGATE
import mozilla.components.concept.engine.request.RequestInterceptor

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
        return !isSubframeRequest && isGoogleRequest.containsMatchIn(uri)
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

        return RequestInterceptor.InterceptionResponse.Url(
            url = uri,
            flags = LoadUrlFlags.select(
                LOAD_FLAGS_BYPASS_LOAD_URI_DELEGATE,
                ALLOW_ADDITIONAL_HEADERS,
            ),
            additionalHeaders = getAdditionalHeaders(isDeviceRamAboveThreshold),
        )
    }
}
