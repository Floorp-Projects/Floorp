/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.concept.engine.history.HistoryTrackingDelegate
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.concept.storage.PageObservation
import mozilla.components.concept.storage.PageVisit

/**
 * Implementation of the [HistoryTrackingDelegate] which delegates work to an instance of [HistoryStorage].
 */
class HistoryDelegate(private val historyStorage: Lazy<HistoryStorage>) : HistoryTrackingDelegate {
    override suspend fun onVisited(uri: String, visit: PageVisit) {
        historyStorage.value.recordVisit(uri, visit)
    }

    override suspend fun onTitleChanged(uri: String, title: String) {
        historyStorage.value.recordObservation(uri, PageObservation(title = title))
    }

    override suspend fun onPreviewImageChange(uri: String, previewImageUrl: String) {
        historyStorage.value.recordObservation(
            uri,
            PageObservation(previewImageUrl = previewImageUrl),
        )
    }

    override suspend fun getVisited(uris: List<String>): List<Boolean> {
        return historyStorage.value.getVisited(uris)
    }

    override suspend fun getVisited(): List<String> {
        return historyStorage.value.getVisited()
    }

    override fun shouldStoreUri(uri: String) = historyStorage.value.canAddUri(uri)
}
