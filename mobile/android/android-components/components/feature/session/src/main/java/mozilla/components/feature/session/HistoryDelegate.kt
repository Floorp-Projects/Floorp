/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import android.net.Uri
import mozilla.components.concept.engine.history.HistoryTrackingDelegate
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.concept.storage.PageObservation
import mozilla.components.concept.storage.VisitType

/**
 * Implementation of the [HistoryTrackingDelegate] which delegates work to an instance of [HistoryStorage].
 */
class HistoryDelegate(private val historyStorage: HistoryStorage) : HistoryTrackingDelegate {
    override suspend fun onVisited(uri: String, type: VisitType) {
        // While we expect engine implementations to check URIs against `shouldStoreUri`, we don't
        // depend on them to actually do this check.
        if (shouldStoreUri(uri)) {
            historyStorage.recordVisit(uri, type)
        }
    }

    override suspend fun onTitleChanged(uri: String, title: String) {
        // Ignore title changes for URIs which we're ignoring.
        if (shouldStoreUri(uri)) {
            historyStorage.recordObservation(uri, PageObservation(title = title))
        }
    }

    override suspend fun getVisited(uris: List<String>): List<Boolean> {
        return historyStorage.getVisited(uris)
    }

    override suspend fun getVisited(): List<String> {
        return historyStorage.getVisited()
    }

    /**
     * Filter out unwanted URIs, such as "chrome:", "about:", etc.
     * Ported from nsAndroidHistory::CanAddURI
     * See https://dxr.mozilla.org/mozilla-central/source/mobile/android/components/build/nsAndroidHistory.cpp#326
     */
    @SuppressWarnings("ReturnCount")
    override fun shouldStoreUri(uri: String): Boolean {
        val parsedUri = Uri.parse(uri)
        val scheme = parsedUri.scheme ?: return false

        // Short-circuit most common schemes.
        if (scheme == "http" || scheme == "https") {
            return true
        }

        // Allow about about:reader uris. They are of the form:
        // about:reader?url=http://some.interesting.page/to/read.html
        if (uri.startsWith("about:reader")) {
            return true
        }

        val schemasToIgnore = listOf(
            "about", "imap", "news", "mailbox", "moz-anno", "moz-extension",
            "view-source", "chrome", "resource", "data", "javascript", "blob"
        )

        return !schemasToIgnore.contains(scheme)
    }
}
