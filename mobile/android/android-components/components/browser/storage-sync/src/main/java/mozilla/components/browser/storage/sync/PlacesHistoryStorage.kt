/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.storage.sync

import android.content.Context
import android.net.Uri
import kotlinx.coroutines.withContext
import mozilla.appservices.places.PlacesApi
import mozilla.appservices.places.uniffi.PlacesException
import mozilla.appservices.places.uniffi.VisitObservation
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.concept.storage.FrecencyThresholdOption
import mozilla.components.concept.storage.HistoryAutocompleteResult
import mozilla.components.concept.storage.HistoryHighlight
import mozilla.components.concept.storage.HistoryHighlightWeights
import mozilla.components.concept.storage.HistoryMetadata
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.concept.storage.HistoryMetadataObservation
import mozilla.components.concept.storage.HistoryMetadataStorage
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.concept.storage.PageObservation
import mozilla.components.concept.storage.PageVisit
import mozilla.components.concept.storage.RedirectSource
import mozilla.components.concept.storage.SearchResult
import mozilla.components.concept.storage.TopFrecentSiteInfo
import mozilla.components.concept.storage.VisitInfo
import mozilla.components.concept.storage.VisitType
import mozilla.components.concept.sync.SyncAuthInfo
import mozilla.components.concept.sync.SyncStatus
import mozilla.components.concept.sync.SyncableStore
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.utils.segmentAwareDomainMatch
import org.json.JSONObject

const val AUTOCOMPLETE_SOURCE_NAME = "placesHistory"

/**
 * Implementation of the [HistoryStorage] which is backed by a Rust Places lib via [PlacesApi].
 */
@Suppress("TooManyFunctions", "LargeClass")
open class PlacesHistoryStorage(
    context: Context,
    crashReporter: CrashReporting? = null
) : PlacesStorage(context, crashReporter), HistoryStorage, HistoryMetadataStorage, SyncableStore {

    override val logger = Logger("PlacesHistoryStorage")

    override suspend fun recordVisit(uri: String, visit: PageVisit) {
        if (!canAddUri(uri)) {
            logger.debug("Not recording visit (canAddUri=false) for: $uri")
            return
        }
        withContext(writeScope.coroutineContext) {
            handlePlacesExceptions("recordVisit") {
                places.writer().noteObservation(
                    VisitObservation(
                        uri,
                        visitType = visit.visitType.intoTransitionType(),
                        isRedirectSource = visit.redirectSource != null,
                        isPermanentRedirectSource = visit.redirectSource == RedirectSource.PERMANENT
                    )
                )
            }
        }
    }

    override suspend fun recordObservation(uri: String, observation: PageObservation) {
        if (!canAddUri(uri)) {
            logger.debug("Not recording observation (canAddUri=false) for: $uri")
            return
        }
        // NB: visitType being null means "record meta information about this URL".
        withContext(writeScope.coroutineContext) {
            // Ignore exceptions related to uris. This means we may drop some of the data on the floor
            // if the underlying storage layer refuses it.
            handlePlacesExceptions("recordObservation") {
                places.writer().noteObservation(
                    VisitObservation(
                        url = uri,
                        visitType = null,
                        title = observation.title,
                        previewImageUrl = observation.previewImageUrl
                    )
                )
            }
        }
    }

    override suspend fun getVisited(uris: List<String>): List<Boolean> {
        return withContext(readScope.coroutineContext) {
            handlePlacesExceptions("getVisited", default = uris.map { false }) {
                places.reader().getVisited(uris)
            }
        }
    }

    override suspend fun getVisited(): List<String> {
        return withContext(readScope.coroutineContext) {
            handlePlacesExceptions("getVisited", default = emptyList()) {
                places.reader().getVisitedUrlsInRange(
                    start = 0,
                    end = System.currentTimeMillis(),
                    includeRemote = true
                )
            }
        }
    }

    override suspend fun getDetailedVisits(start: Long, end: Long, excludeTypes: List<VisitType>): List<VisitInfo> {
        return withContext(readScope.coroutineContext) {
            handlePlacesExceptions("getDetailedVisits", default = emptyList()) {
                places.reader().getVisitInfos(start, end, excludeTypes.map { it.into() }).map { it.into() }
            }
        }
    }

    override suspend fun getVisitsPaginated(offset: Long, count: Long, excludeTypes: List<VisitType>): List<VisitInfo> {
        return withContext(readScope.coroutineContext) {
            handlePlacesExceptions("getVisitsPaginated", default = emptyList()) {
                places.reader().getVisitPage(offset, count, excludeTypes.map { it.into() }).map { it.into() }
            }
        }
    }

    override suspend fun getTopFrecentSites(
        numItems: Int,
        frecencyThreshold: FrecencyThresholdOption
    ): List<TopFrecentSiteInfo> {
        if (numItems <= 0) {
            return emptyList()
        }

        return withContext(readScope.coroutineContext) {
            handlePlacesExceptions("getTopFrecentSites", default = emptyList()) {
                places.reader().getTopFrecentSiteInfos(numItems, frecencyThreshold.into())
                    .map { it.into() }
            }
        }
    }

    override fun getSuggestions(query: String, limit: Int): List<SearchResult> {
        require(limit >= 0) { "Limit must be a positive integer" }
        return handlePlacesExceptions("getSuggestions", default = emptyList()) {
            places.reader().queryAutocomplete(query, limit = limit).map {
                SearchResult(it.url, it.url, it.frecency.toInt(), it.title)
            }
        }
    }

    override fun getAutocompleteSuggestion(query: String): HistoryAutocompleteResult? {
        val url = handlePlacesExceptions("getAutoCompleteSuggestions", default = null) {
            places.reader().matchUrl(query)
        } ?: return null

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
            handlePlacesExceptions("deleteEverything") {
                places.writer().deleteEverything()
            }
        }
    }

    /**
     * Sync behaviour: may remove history from remote devices, if the removed visits were the only
     * ones for a URL.
     */
    override suspend fun deleteVisitsSince(since: Long) {
        withContext(writeScope.coroutineContext) {
            handlePlacesExceptions("deleteVisitsSince") {
                places.writer().deleteVisitsSince(since)
            }
        }
    }

    /**
     * Sync behaviour: may remove history from remote devices, if the removed visits were the only
     * ones for a URL.
     */
    override suspend fun deleteVisitsBetween(startTime: Long, endTime: Long) {
        withContext(writeScope.coroutineContext) {
            handlePlacesExceptions("deleteVisitsBetween") {
                places.writer().deleteVisitsBetween(startTime, endTime)
            }
        }
    }

    /**
     * Sync behaviour: will remove history from remote devices.
     */
    override suspend fun deleteVisitsFor(url: String) {
        withContext(writeScope.coroutineContext) {
            handlePlacesExceptions("deleteVisitsFor") {
                places.writer().deleteVisitsFor(url)
            }
        }
    }

    /**
     * Sync behaviour: will remove history from remote devices if this was the only visit for [url].
     * Otherwise, remote devices are not affected.
     */
    override suspend fun deleteVisit(url: String, timestamp: Long) {
        withContext(writeScope.coroutineContext) {
            handlePlacesExceptions("deleteVisit") {
                places.writer().deleteVisit(url, timestamp)
            }
        }
    }

    /**
     * Should only be called in response to severe disk storage pressure. May delete all of the data,
     * or some subset of it.
     * Sync behaviour: will not remove history from remote clients.
     */
    override suspend fun prune() {
        withContext(writeScope.coroutineContext) {
            handlePlacesExceptions("prune") {
                places.writer().pruneDestructively()
            }
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

    override fun registerWithSyncManager() {
        return places.registerWithSyncManager()
    }

    override suspend fun getLatestHistoryMetadataForUrl(url: String): HistoryMetadata? {
        return handlePlacesExceptions("getLatestHistoryMetadataForUrl", default = null) {
            places.reader().getLatestHistoryMetadataForUrl(url)?.into()
        }
    }

    override suspend fun getHistoryMetadataSince(since: Long): List<HistoryMetadata> {
        return handlePlacesExceptions("getHistoryMetadataSince", default = emptyList()) {
            places.reader().getHistoryMetadataSince(since).into()
        }
    }

    override suspend fun getHistoryMetadataBetween(start: Long, end: Long): List<HistoryMetadata> {
        return handlePlacesExceptions("getHistoryMetadataBetween", default = emptyList()) {
            places.reader().getHistoryMetadataBetween(start, end).into()
        }
    }

    override suspend fun queryHistoryMetadata(query: String, limit: Int): List<HistoryMetadata> {
        return handlePlacesExceptions("queryHistoryMetadata", default = emptyList()) {
            places.reader().queryHistoryMetadata(query, limit).into()
        }
    }

    override suspend fun getHistoryHighlights(
        weights: HistoryHighlightWeights,
        limit: Int
    ): List<HistoryHighlight> {
        return handlePlacesExceptions("getHistoryHighlights", default = emptyList()) {
            places.reader().getHighlights(weights.into(), limit).intoHighlights()
        }
    }

    override suspend fun noteHistoryMetadataObservation(
        key: HistoryMetadataKey,
        observation: HistoryMetadataObservation
    ) {
        if (!canAddUri(key.url)) {
            logger.debug("Not recording metadata (canAddUri=false) for: ${key.url}")
            return
        }
        withContext(writeScope.coroutineContext) {
            handlePlacesExceptions("noteHistoryMetadataObservation") {
                places.writer().noteHistoryMetadataObservation(observation.into(key))
            }
        }
    }

    override suspend fun deleteHistoryMetadataOlderThan(olderThan: Long) {
        withContext(writeScope.coroutineContext) {
            handlePlacesExceptions("deleteHistoryMetadataOlderThan") {
                places.writer().deleteHistoryMetadataOlderThan(olderThan)
            }
        }
    }

    override suspend fun deleteHistoryMetadata(key: HistoryMetadataKey) {
        withContext(writeScope.coroutineContext) {
            handlePlacesExceptions("deleteHistoryMetadata") {
                places.writer().deleteHistoryMetadata(key.into())
            }
        }
    }

    override suspend fun deleteHistoryMetadata(searchTerm: String) {
        deleteHistoryMetadata {
            // NB: searchTerms are always lower-case in the database.
            it.searchTerm == searchTerm.lowercase()
        }
    }

    override suspend fun deleteHistoryMetadataForUrl(url: String) {
        deleteHistoryMetadata {
            it.url == url
        }
    }

    private suspend fun deleteHistoryMetadata(
        predicate: (mozilla.appservices.places.uniffi.HistoryMetadata) -> Boolean
    ) {
        // Ideally, we want this to live in A-S as a simple DELETE statement.
        // As-is, this isn't an atomic operation. For how we're using these data, both lack of
        // atomicity and a performance penalty is acceptable for now.
        withContext(writeScope.coroutineContext) {
            handlePlacesExceptions("deleteHistoryMetadata") {
                places.reader().getHistoryMetadataSince(Long.MIN_VALUE)
                    .filter(predicate)
                    .forEach {
                        places.writer().deleteHistoryMetadata(
                            HistoryMetadataKey(
                                url = it.url,
                                searchTerm = it.searchTerm,
                                referrerUrl = it.referrerUrl
                            ).into()
                        )
                    }
            }
        }
    }

    @SuppressWarnings("ReturnCount")
    override fun canAddUri(uri: String): Boolean {
        // Filter out unwanted URIs, such as "chrome:", "about:", etc.
        // Ported from nsAndroidHistory::CanAddURI
        // See https://dxr.mozilla.org/mozilla-central/source/mobile/android/components/build/nsAndroidHistory.cpp#326
        val parsedUri = Uri.parse(uri)
        val scheme = parsedUri.normalizeScheme().scheme ?: return false

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
