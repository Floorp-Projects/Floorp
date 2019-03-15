/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.storage.sync

import android.content.Context
import android.support.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancelChildren
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.appservices.places.PlacesException
import mozilla.appservices.places.VisitObservation
import mozilla.components.concept.storage.HistoryAutocompleteResult
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.concept.storage.PageObservation
import mozilla.components.concept.storage.SearchResult
import mozilla.components.concept.storage.VisitInfo
import mozilla.components.concept.sync.SyncStatus
import mozilla.components.concept.sync.SyncableStore
import mozilla.components.concept.storage.VisitType
import mozilla.components.concept.sync.AuthInfo
import mozilla.components.support.utils.segmentAwareDomainMatch

const val AUTOCOMPLETE_SOURCE_NAME = "placesHistory"

typealias SyncAuthInfo = mozilla.appservices.places.SyncAuthInfo

/**
 * Implementation of the [HistoryStorage] which is backed by a Rust Places lib via [PlacesConnection].
 */
open class PlacesHistoryStorage(context: Context) : HistoryStorage, SyncableStore {
    private val scope by lazy { CoroutineScope(Dispatchers.IO) }
    private val storageDir by lazy { context.filesDir }

    @VisibleForTesting
    internal open val places: Connection by lazy {
        RustPlacesConnection.init(storageDir)
        RustPlacesConnection
    }

    override suspend fun recordVisit(uri: String, visitType: VisitType) {
        withContext(scope.coroutineContext) {
            places.writer().noteObservation(VisitObservation(uri, visitType = visitType.into()))
        }
    }

    override suspend fun recordObservation(uri: String, observation: PageObservation) {
        // NB: visitType 'UPDATE_PLACE' means "record meta information about this URL".
        withContext(scope.coroutineContext) {
            places.writer().noteObservation(
                VisitObservation(
                    url = uri,
                    visitType = mozilla.appservices.places.VisitType.UPDATE_PLACE,
                    title = observation.title
                )
            )
        }
    }

    override suspend fun getVisited(uris: List<String>): List<Boolean> {
        return withContext(scope.coroutineContext) { places.reader().getVisited(uris) }
    }

    override suspend fun getVisited(): List<String> {
        return withContext(scope.coroutineContext) {
            places.reader().getVisitedUrlsInRange(
                start = 0,
                end = System.currentTimeMillis(),
                includeRemote = true
            )
        }
    }

    override suspend fun getDetailedVisits(start: Long, end: Long): List<VisitInfo> {
        return withContext(scope.coroutineContext) {
            places.reader().getVisitInfos(start, end).map { it.into() }
        }
    }

    override fun cleanup() {
        scope.coroutineContext.cancelChildren()
        places.close()
    }

    override fun getSuggestions(query: String, limit: Int): List<SearchResult> {
        require(limit >= 0) { "Limit must be a positive integer" }
        return places.reader().queryAutocomplete(query, limit = limit).map {
            SearchResult(it.url, it.url, it.frecency.toInt(), it.title)
        }
    }

    override fun getAutocompleteSuggestion(query: String): HistoryAutocompleteResult? {
        val url = places.reader().matchUrl(query) ?: return null

        val resultText = segmentAwareDomainMatch(query, arrayListOf(url))
        return resultText?.let {
            HistoryAutocompleteResult(
                input = query,
                text = it.matchedSegment,
                url = it.url,
                source = AUTOCOMPLETE_SOURCE_NAME,
                totalItems = 1
            )
        }
    }

    override suspend fun sync(authInfo: AuthInfo): SyncStatus {
        return try {
            places.sync(authInfo.into())
            SyncStatus.Ok
        } catch (e: PlacesException) {
            SyncStatus.Error(e)
        }
    }
}
