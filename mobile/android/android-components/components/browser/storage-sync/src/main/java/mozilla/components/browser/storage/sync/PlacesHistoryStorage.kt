/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.storage.sync

import android.content.Context
import kotlinx.coroutines.withContext
import mozilla.appservices.places.PlacesApi
import mozilla.appservices.places.PlacesException
import mozilla.appservices.places.VisitObservation
import mozilla.components.concept.storage.HistoryAutocompleteResult
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.concept.storage.PageObservation
import mozilla.components.concept.storage.PageVisit
import mozilla.components.concept.storage.SearchResult
import mozilla.components.concept.storage.RedirectSource
import mozilla.components.concept.storage.TopFrecentSiteInfo
import mozilla.components.concept.storage.VisitInfo
import mozilla.components.concept.storage.VisitType
import mozilla.components.concept.sync.SyncAuthInfo
import mozilla.components.concept.sync.SyncStatus
import mozilla.components.concept.sync.SyncableStore
import mozilla.components.support.base.crash.CrashReporting
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.utils.segmentAwareDomainMatch
import org.json.JSONObject

const val AUTOCOMPLETE_SOURCE_NAME = "placesHistory"

/**
 * Implementation of the [HistoryStorage] which is backed by a Rust Places lib via [PlacesApi].
 */
@SuppressWarnings("TooManyFunctions")
open class PlacesHistoryStorage(
    context: Context,
    crashReporter: CrashReporting? = null
) : PlacesStorage(context, crashReporter), HistoryStorage, SyncableStore {

    override val logger = Logger("PlacesHistoryStorage")

    override suspend fun recordVisit(uri: String, visit: PageVisit) {
        withContext(writeScope.coroutineContext) {
            handlePlacesExceptions("recordVisit") {
                places.writer().noteObservation(VisitObservation(uri,
                    visitType = visit.visitType.into(),
                    isRedirectSource = when (visit.redirectSource) {
                        RedirectSource.PERMANENT, RedirectSource.TEMPORARY -> true
                        RedirectSource.NOT_A_SOURCE -> false
                    },
                    isPermanentRedirectSource = when (visit.redirectSource) {
                        RedirectSource.PERMANENT -> true
                        RedirectSource.TEMPORARY, RedirectSource.NOT_A_SOURCE -> false
                    }
                ))
            }
        }
    }

    override suspend fun recordObservation(uri: String, observation: PageObservation) {
        // NB: visitType 'UPDATE_PLACE' means "record meta information about this URL".
        withContext(writeScope.coroutineContext) {
            // Ignore exceptions related to uris. This means we may drop some of the data on the floor
            // if the underlying storage layer refuses it.
            handlePlacesExceptions("recordObservation") {
                places.writer().noteObservation(
                        VisitObservation(
                                url = uri,
                                visitType = mozilla.appservices.places.VisitType.UPDATE_PLACE,
                                title = observation.title
                        )
                )
            }
        }
    }

    override suspend fun getVisited(uris: List<String>): List<Boolean> {
        return withContext(readScope.coroutineContext) { places.reader().getVisited(uris) }
    }

    override suspend fun getVisited(): List<String> {
        return withContext(readScope.coroutineContext) {
            places.reader().getVisitedUrlsInRange(
                    start = 0,
                    end = System.currentTimeMillis(),
                    includeRemote = true
            )
        }
    }

    override suspend fun getDetailedVisits(start: Long, end: Long, excludeTypes: List<VisitType>): List<VisitInfo> {
        return withContext(readScope.coroutineContext) {
            places.reader().getVisitInfos(start, end, excludeTypes.map { it.into() }).map { it.into() }
        }
    }

    override suspend fun getVisitsPaginated(offset: Long, count: Long, excludeTypes: List<VisitType>): List<VisitInfo> {
        return withContext(readScope.coroutineContext) {
            places.reader().getVisitPage(offset, count, excludeTypes.map { it.into() }).map { it.into() }
        }
    }

    override suspend fun getTopFrecentSites(numItems: Int): List<TopFrecentSiteInfo> {
        return withContext(readScope.coroutineContext) {
            places.reader().getTopFrecentSiteInfos(numItems).map { it.into() }
        }
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

    /**
     * Sync behaviour: will not remove any history from remote devices, but it will prevent deleted
     * history from returning.
     */
    override suspend fun deleteEverything() {
        withContext(writeScope.coroutineContext) {
            places.writer().deleteEverything()
        }
    }

    /**
     * Sync behaviour: may remove history from remote devices, if the removed visits were the only
     * ones for a URL.
     */
    override suspend fun deleteVisitsSince(since: Long) {
        withContext(writeScope.coroutineContext) {
            places.writer().deleteVisitsSince(since)
        }
    }

    /**
     * Sync behaviour: may remove history from remote devices, if the removed visits were the only
     * ones for a URL.
     */
    override suspend fun deleteVisitsBetween(startTime: Long, endTime: Long) {
        withContext(writeScope.coroutineContext) {
            places.writer().deleteVisitsBetween(startTime, endTime)
        }
    }

    /**
     * Sync behaviour: will remove history from remote devices.
     */
    override suspend fun deleteVisitsFor(url: String) {
        withContext(writeScope.coroutineContext) {
            places.writer().deleteVisitsFor(url)
        }
    }

    /**
     * Sync behaviour: will remove history from remote devices if this was the only visit for [url].
     * Otherwise, remote devices are not affected.
     */
    override suspend fun deleteVisit(url: String, timestamp: Long) {
        withContext(writeScope.coroutineContext) {
            places.writer().deleteVisit(url, timestamp)
        }
    }

    /**
     * Should only be called in response to severe disk storage pressure. May delete all of the data,
     * or some subset of it.
     * Sync behaviour: will not remove history from remote clients.
     */
    override suspend fun prune() {
        withContext(writeScope.coroutineContext) {
            places.writer().pruneDestructively()
        }
    }

    /**
     * Runs syncHistory() method on the places Connection
     *
     * @param authInfo The authentication information to sync with.
     * @return Sync status of OK or Error
     */
    suspend fun sync(authInfo: SyncAuthInfo): SyncStatus {
        return withContext(writeScope.coroutineContext) {
            syncAndHandleExceptions {
                places.syncHistory(authInfo)
            }
        }
    }

    /**
     * Import history and visits data from Fennec's browser.db file.
     *
     * @param dbPath Absolute path to Fennec's browser.db file.
     * @return Migration metrics wrapped in a JSON object. See libplaces for schema details.
     */
    @Throws(PlacesException::class)
    fun importFromFennec(dbPath: String): JSONObject {
        return places.importVisitsFromFennec(dbPath)
    }

    /**
     * This should be removed. See: https://github.com/mozilla/application-services/issues/1877
     *
     * @return raw internal handle that could be used for referencing underlying [PlacesApi]. Use it with SyncManager.
     */
    override fun getHandle(): Long {
        return places.getHandle()
    }
}
