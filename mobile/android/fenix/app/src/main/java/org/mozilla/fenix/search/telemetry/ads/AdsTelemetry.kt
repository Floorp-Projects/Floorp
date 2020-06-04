/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.telemetry.ads

import androidx.annotation.VisibleForTesting
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import org.json.JSONObject
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.search.telemetry.BaseSearchTelemetry
import org.mozilla.fenix.search.telemetry.ExtensionInfo

class AdsTelemetry(private val metrics: MetricController) : BaseSearchTelemetry() {

    override fun install(
        engine: Engine,
        store: BrowserStore
    ) {
        val info = ExtensionInfo(
            id = ADS_EXTENSION_ID,
            resourceUrl = ADS_EXTENSION_RESOURCE_URL,
            messageId = ADS_MESSAGE_ID
        )
        installWebExtension(engine, store, info)
    }

    fun trackAdClickedMetric(sessionUrl: String?, urlPath: List<String>) {
        if (sessionUrl == null) {
            return
        }
        val provider = getProviderForUrl(sessionUrl)
        provider?.let {
            if (it.containsAds(urlPath)) {
                metrics.track(Event.SearchAdClicked(it.name))
            }
        }
    }

    override fun processMessage(message: JSONObject) {
        val urls = getMessageList<String>(message, ADS_MESSAGE_DOCUMENT_URLS_KEY)
        val provider = getProviderForUrl(message.getString(ADS_MESSAGE_SESSION_URL_KEY))
        provider?.let {
            if (it.containsAds(urls)) {
                metrics.track(Event.SearchWithAds(it.name))
            }
        }
    }

    companion object {
        @VisibleForTesting
        internal const val ADS_EXTENSION_ID = "ads@mozac.org"
        @VisibleForTesting
        internal const val ADS_EXTENSION_RESOURCE_URL = "resource://android/assets/extensions/ads/"
        @VisibleForTesting
        internal const val ADS_MESSAGE_SESSION_URL_KEY = "url"
        @VisibleForTesting
        internal const val ADS_MESSAGE_DOCUMENT_URLS_KEY = "urls"
        @VisibleForTesting
        internal const val ADS_MESSAGE_ID = "MozacBrowserAds"
    }
}
