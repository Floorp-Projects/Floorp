/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.telemetry.incontent

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
 * Telemetry for knowing of in-web-content searches (including follow-on searches) and the provider used.
 *
 * Implemented as a browser extension based on the WebExtension API:
 * https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions
 */
class InContentTelemetry : BaseSearchTelemetry() {

    override fun install(engine: Engine, store: BrowserStore) {
        val info = ExtensionInfo(
            id = SEARCH_EXTENSION_ID,
            resourceUrl = SEARCH_EXTENSION_RESOURCE_URL,
            messageId = SEARCH_MESSAGE_ID,
        )
        installWebExtension(engine, store, info)
    }

    override fun processMessage(message: JSONObject) {
        val cookies = message.getJSONArray(SEARCH_MESSAGE_LIST_KEY).toList<JSONObject>()
        trackPartnerUrlTypeMetric(message.getString(SEARCH_MESSAGE_SESSION_URL_KEY), cookies)
    }

    @VisibleForTesting
    internal fun trackPartnerUrlTypeMetric(url: String, cookies: List<JSONObject>) {
        val provider = getProviderForUrl(url) ?: return
        val uri = Uri.parse(url)
        val paramSet = uri.queryParameterNames

        if (!paramSet.contains(provider.queryParam)) {
            return
        }

        emitFact(
            IN_CONTENT_SEARCH,
            getTrackKey(provider, uri, cookies),
        )
    }

    companion object {
        /**
         * [Fact] property indicating that the user did a search, be it a new one
         * or continuing from an existing search.
         */
        const val IN_CONTENT_SEARCH = "in content search"

        @VisibleForTesting
        internal const val SEARCH_EXTENSION_ID = "cookies@mozac.org"

        @VisibleForTesting
        internal const val SEARCH_EXTENSION_RESOURCE_URL = "resource://android/assets/extensions/search/"

        @VisibleForTesting
        internal const val SEARCH_MESSAGE_SESSION_URL_KEY = "url"

        @VisibleForTesting
        internal const val SEARCH_MESSAGE_LIST_KEY = "cookies"

        @VisibleForTesting
        internal const val SEARCH_MESSAGE_ID = "MozacBrowserSearchMessage"
    }
}
