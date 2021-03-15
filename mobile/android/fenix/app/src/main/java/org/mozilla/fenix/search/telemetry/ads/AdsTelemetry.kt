/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.telemetry.ads

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

class AdsTelemetry(private val metrics: MetricController) : BaseSearchTelemetry() {

    // Cache the cookies provided by the ADS_EXTENSION_ID extension to be used when tracking
    // the Ads clicked telemetry.
    var cachedCookies = listOf<JSONObject>()

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

    override fun processMessage(message: JSONObject) {
        // Cache the cookies list when the extension sends a message.
        cachedCookies = getMessageList(
            message,
            ADS_MESSAGE_COOKIES_KEY
        )

        val urls = getMessageList<String>(message, ADS_MESSAGE_DOCUMENT_URLS_KEY)
        val provider = getProviderForUrl(message.getString(ADS_MESSAGE_SESSION_URL_KEY))

        provider?.let {
            if (it.containsAdLinks(urls)) {
                metrics.track(Event.SearchWithAds(it.name))
            }
        }
    }

    /**
     * If a search ad is clicked, record the search ad that was clicked. This method is called
     * when the browser is navigating to a new URL, which may be a search ad.
     *
     * @param url The URL of the page before the search ad was clicked. This is used to determine
     * the originating search provider.
     * @param urlPath A list of the URLs and load requests collected in between location changes.
     * Clicking on a search ad generates a list of redirects from the originating search provider
     * to the ad source. This is used to determine if there was an ad click.
     */
    fun trackAdClickedMetric(url: String?, urlPath: List<String>) {
        val uri = url?.toUri() ?: return
        val provider = getProviderForUrl(url) ?: return
        val paramSet = uri.queryParameterNames

        if (!paramSet.contains(provider.queryParam) || !provider.containsAdLinks(urlPath)) {
            // Do nothing if the URL does not have the search provider's query parameter or
            // there were no ad clicks.
            return
        }

        metrics.track(Event.SearchAdClicked(getTrackKey(provider, uri, cachedCookies)))
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

        @VisibleForTesting
        internal const val ADS_MESSAGE_COOKIES_KEY = "cookies"
    }
}
