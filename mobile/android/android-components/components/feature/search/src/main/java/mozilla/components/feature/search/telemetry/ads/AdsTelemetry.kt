/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.telemetry.ads

import android.net.Uri
import androidx.annotation.VisibleForTesting
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.search.telemetry.BaseSearchTelemetry
import mozilla.components.feature.search.telemetry.ExtensionInfo
import mozilla.components.feature.search.telemetry.getTrackKey
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.ktx.android.org.json.toList
import org.json.JSONObject

/**
 * Telemetry for knowing how often users see/click ads in search and from which provider.
 *
 * Implemented as a browser extension based on the WebExtension API:
 * https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions
 */
class AdsTelemetry : BaseSearchTelemetry() {

    // SERP cached cookies used to check whether an ad was clicked.
    @VisibleForTesting
    internal var cachedCookies = listOf<JSONObject>()

    override fun install(
        engine: Engine,
        store: BrowserStore,
    ) {
        val info = ExtensionInfo(
            id = ADS_EXTENSION_ID,
            resourceUrl = ADS_EXTENSION_RESOURCE_URL,
            messageId = ADS_MESSAGE_ID,
        )
        installWebExtension(engine, store, info)
    }

    override fun processMessage(message: JSONObject) {
        // Cache the cookies list when the extension sends a message.
        cachedCookies = message.getJSONArray(ADS_MESSAGE_COOKIES_KEY).toList()

        val urls = message.getJSONArray(ADS_MESSAGE_DOCUMENT_URLS_KEY).toList<String>()
        val provider = getProviderForUrl(message.getString(ADS_MESSAGE_SESSION_URL_KEY))

        provider?.let {
            if (it.containsAdLinks(urls)) {
                emitFact(
                    SERP_SHOWN_WITH_ADDS,
                    it.name,
                )
            }
        }
    }

    /**
     * To be called when the browser is navigating to a new URL, which may be a search ad.
     *
     * @param url The URL of the page before the search ad was clicked.
     * This will be used to determine the originating search provider.
     * @param urlPath A list of the URLs and load requests collected in between location changes.
     * Clicking on a search ad generates a list of redirects from the originating search provider
     * to the ad source. This is used to determine if there was an ad click.
     */
    @Suppress("ReturnCount")
    fun checkIfAddWasClicked(url: String?, urlPath: List<String>) {
        if (url == null) {
            return
        }
        val uri = Uri.parse(url) ?: return
        val provider = getProviderForUrl(url) ?: return
        val paramSet = uri.queryParameterNames

        if (!paramSet.contains(provider.queryParam) || !provider.containsAdLinks(urlPath)) {
            // Do nothing if the URL does not have the search provider's query parameter or
            // there were no ad clicks.
            return
        }

        emitFact(
            SERP_ADD_CLICKED,
            getTrackKey(provider, uri, cachedCookies),
        )
    }

    companion object {
        /**
         * [Fact] property indicating the user open a Search Engine Result Page
         * of one of our search providers which contains ads.
         */
        const val SERP_SHOWN_WITH_ADDS = "SERP shown with adds"

        /**
         * [Fact] property indicating that an ad was clicked in a Search Engine Result Page.
         */
        const val SERP_ADD_CLICKED = "SERP add clicked"

        @VisibleForTesting
        internal const val ADS_EXTENSION_ID = "ads@mozac.org"

        @VisibleForTesting
        internal const val ADS_EXTENSION_RESOURCE_URL = "resource://android/assets/extensions/ads/"

        @VisibleForTesting
        internal const val ADS_MESSAGE_SESSION_URL_KEY = "url"

        @VisibleForTesting
        internal const val ADS_MESSAGE_DOCUMENT_URLS_KEY = "urls"

        @VisibleForTesting
        internal const val ADS_MESSAGE_COOKIES_KEY = "cookies"

        @VisibleForTesting
        internal const val ADS_MESSAGE_ID = "MozacBrowserAdsMessage"
    }
}
