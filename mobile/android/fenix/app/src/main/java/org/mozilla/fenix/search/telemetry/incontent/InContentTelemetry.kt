/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.telemetry.incontent

import android.net.Uri
import androidx.annotation.VisibleForTesting
import androidx.core.net.toUri
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import org.json.JSONObject
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.search.telemetry.BaseSearchTelemetry
import org.mozilla.fenix.search.telemetry.ExtensionInfo
import org.mozilla.fenix.search.telemetry.SearchProviderModel

class InContentTelemetry(private val metrics: MetricController) : BaseSearchTelemetry() {

    override fun install(engine: Engine, store: BrowserStore) {
        val info = ExtensionInfo(
            id = COOKIES_EXTENSION_ID,
            resourceUrl = COOKIES_EXTENSION_RESOURCE_URL,
            messageId = COOKIES_MESSAGE_ID
        )
        installWebExtension(engine, store, info)
    }

    override fun processMessage(message: JSONObject) {
        val cookies = getMessageList<JSONObject>(
            message,
            COOKIES_MESSAGE_LIST_KEY
        )
        trackPartnerUrlTypeMetric(message.getString(COOKIES_MESSAGE_SESSION_URL_KEY), cookies)
    }

    @VisibleForTesting
    internal fun trackPartnerUrlTypeMetric(url: String, cookies: List<JSONObject>) {
        val provider = getProviderForUrl(url)
        var trackKey: TrackKeyInfo? = null

        provider?.let {
            val uri = url.toUri()
            val paramSet = uri.queryParameterNames
            if (!paramSet.contains(provider.queryParam)) {
                return
            }
            var code: String? = null

            if (provider.codeParam.isNotEmpty()) {
                code = uri.getQueryParameter(provider.codeParam)
                // Try cookies first because Bing has followOnCookies and valid code, but no
                // followOnParams => would tracks organic instead of sap-follow-on
                if (provider.followOnCookies.isNotEmpty()) {
                    // Checks if engine contains a valid follow-on cookie, otherwise return default
                    trackKey = getTrackKeyFromCookies(provider, uri, cookies, code)
                }

                // For Bing if it didn't have a valid cookie and for all the other search engines
                if (resultNotComputedFromCookies(trackKey) && hasValidCode(code, provider)) {
                    val type = getSapType(provider.followOnParams, paramSet)
                    trackKey = TrackKeyInfo(provider.name, type, code)
                }
            }

            // Go default if no codeParam was found
            if (trackKey == null) {
                trackKey = TrackKeyInfo(provider.name, SEARCH_TYPE_ORGANIC, code)
            }

            trackKey?.let {
                metrics.track(Event.SearchInContent(it.createTrackKey()))
            }
        }
    }

    private fun resultNotComputedFromCookies(trackKey: TrackKeyInfo?): Boolean =
        trackKey == null || trackKey.type == SEARCH_TYPE_ORGANIC

    private fun hasValidCode(code: String?, provider: SearchProviderModel): Boolean =
        code != null && provider.codePrefixes.any { prefix -> code.startsWith(prefix) }

    private fun getSapType(followOnParams: List<String>, paramSet: Set<String>): String {
        return if (followOnParams.any { param -> paramSet.contains(param) }) {
            SEARCH_TYPE_SAP_FOLLOW_ON
        } else {
            SEARCH_TYPE_SAP
        }
    }

    private fun getTrackKeyFromCookies(
        provider: SearchProviderModel,
        uri: Uri,
        cookies: List<JSONObject>,
        code: String?
    ): TrackKeyInfo {
        // Especially Bing requires lots of extra work related to cookies.
        for (followOnCookie in provider.followOnCookies) {
            val eCode = uri.getQueryParameter(followOnCookie.extraCodeParam)
            if (eCode == null || !followOnCookie.extraCodePrefixes.any { prefix ->
                    eCode.startsWith(prefix)
                }) {
                continue
            }

            // If this cookie is present, it's probably an SAP follow-on.
            // This might be an organic follow-on in the same session, but there
            // is no way to tell the difference.
            for (cookie in cookies) {
                if (cookie.getString("name") != followOnCookie.name) {
                    continue
                }
                val valueList = cookie.getString("value")
                    .split("=")
                    .map { item -> item.trim() }

                if (valueList.size == 2 && valueList[0] == followOnCookie.codeParam &&
                    followOnCookie.codePrefixes.any { prefix ->
                        valueList[1].startsWith(
                            prefix
                        )
                    }
                ) {
                    return TrackKeyInfo(provider.name, SEARCH_TYPE_SAP_FOLLOW_ON, valueList[1])
                }
            }
        }
        return TrackKeyInfo(provider.name, SEARCH_TYPE_ORGANIC, code)
    }

    companion object {
        @VisibleForTesting
        internal const val COOKIES_EXTENSION_ID = "cookies@mozac.org"
        @VisibleForTesting
        internal const val COOKIES_EXTENSION_RESOURCE_URL =
            "resource://android/assets/extensions/cookies/"
        @VisibleForTesting
        internal const val COOKIES_MESSAGE_SESSION_URL_KEY = "url"
        @VisibleForTesting
        internal const val COOKIES_MESSAGE_LIST_KEY = "cookies"
        @VisibleForTesting
        internal const val COOKIES_MESSAGE_ID = "BrowserCookiesMessage"

        private const val SEARCH_TYPE_ORGANIC = "organic"
        private const val SEARCH_TYPE_SAP = "sap"
        private const val SEARCH_TYPE_SAP_FOLLOW_ON = "sap-follow-on"
    }
}
