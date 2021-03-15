/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.telemetry.incontent

import androidx.annotation.VisibleForTesting
import androidx.core.net.toUri
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import org.json.JSONObject
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.search.telemetry.BaseSearchTelemetry
import org.mozilla.fenix.search.telemetry.ExtensionInfo
import org.mozilla.fenix.search.telemetry.getTrackKey

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
        val provider = getProviderForUrl(url) ?: return
        val uri = url.toUri()
        val paramSet = uri.queryParameterNames

        if (!paramSet.contains(provider.queryParam)) {
            return
        }

        metrics.track(Event.SearchInContent(getTrackKey(provider, uri, cookies)))
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
    }
}
