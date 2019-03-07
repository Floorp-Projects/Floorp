/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.storage.memory

import androidx.annotation.VisibleForTesting
import mozilla.components.concept.storage.HistoryAutocompleteResult
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.concept.storage.PageObservation
import mozilla.components.concept.storage.SearchResult
import mozilla.components.concept.storage.VisitInfo
import mozilla.components.concept.storage.VisitType
import mozilla.components.support.utils.StorageUtils.levenshteinDistance
import mozilla.components.support.utils.segmentAwareDomainMatch

data class Visit(val timestamp: Long, val type: VisitType)

const val AUTOCOMPLETE_SOURCE_NAME = "memoryHistory"

/**
 * An in-memory implementation of [mozilla.components.concept.storage.HistoryStorage].
 */
@SuppressWarnings("TooManyFunctions")
class InMemoryHistoryStorage : HistoryStorage {
    @VisibleForTesting
    internal var pages: HashMap<String, MutableList<Visit>> = linkedMapOf()
    @VisibleForTesting
    internal val pageMeta: HashMap<String, PageObservation> = hashMapOf()

    override suspend fun recordVisit(uri: String, visitType: VisitType) {
        val now = System.currentTimeMillis()

        synchronized(pages) {
            if (!pages.containsKey(uri)) {
                pages[uri] = mutableListOf(Visit(now, visitType))
            } else {
                pages[uri]!!.add(Visit(now, visitType))
            }
        }
    }

    override suspend fun recordObservation(uri: String, observation: PageObservation) = synchronized(pageMeta) {
        pageMeta[uri] = observation
    }

    override suspend fun getVisited(uris: List<String>): List<Boolean> = synchronized(pages) {
        return uris.map {
            if (pages[it] != null && pages[it]!!.size > 0) {
                return@map true
            }
            return@map false
        }
    }

    override suspend fun getVisited(): List<String> = synchronized(pages) {
        return pages.keys.toList()
    }

    override suspend fun getVisitsPaginated(offset: Long, count: Long, excludeTypes: List<VisitType>): List<VisitInfo> {
        throw UnsupportedOperationException("Pagination is not yet supported by the in-memory history storage")
    }

    override suspend fun getDetailedVisits(
        start: Long,
        end: Long,
        excludeTypes: List<VisitType>
    ): List<VisitInfo> = synchronized(pages + pageMeta) {
        val visits = mutableListOf<VisitInfo>()

        pages.forEach {
            it.value.forEach { visit ->
                if (visit.timestamp >= start && visit.timestamp <= end && !excludeTypes.contains(visit.type)) {
                    visits.add(VisitInfo(
                        url = it.key,
                        title = pageMeta[it.key]?.title,
                        visitTime = visit.timestamp,
                        visitType = visit.type
                    ))
                }
            }
        }

        return visits
    }

    override fun getSuggestions(query: String, limit: Int): List<SearchResult> = synchronized(pages + pageMeta) {
        data class Hit(val url: String, val score: Int)

        val urlMatches = pages.asSequence().map {
            Hit(it.key, levenshteinDistance(it.key, query))
        }
        val titleMatches = pageMeta.asSequence().map {
            Hit(it.key, levenshteinDistance(it.value.title ?: "", query))
        }
        val matchedUrls = mutableMapOf<String, Int>()
        urlMatches.plus(titleMatches).forEach {
            if (matchedUrls.containsKey(it.url) && matchedUrls[it.url]!! < it.score) {
                matchedUrls[it.url] = it.score
            } else {
                matchedUrls[it.url] = it.score
            }
        }
        // Calculate maxScore so that we can invert our scoring.
        // Lower Levenshtein distance should produce a higher score.
        val maxScore = urlMatches.maxBy { it.score }?.score ?: return@synchronized listOf()

        // TODO exclude non-matching results entirely? Score that implies complete mismatch.
        matchedUrls.asSequence().sortedBy { it.value }.map {
            SearchResult(id = it.key, score = maxScore - it.value, url = it.key, title = pageMeta[it.key]?.title)
        }.take(limit).toList()
    }

    override fun getAutocompleteSuggestion(query: String): HistoryAutocompleteResult? = synchronized(pages) {
        segmentAwareDomainMatch(query, pages.keys)?.let { urlMatch ->
            return HistoryAutocompleteResult(
                query, urlMatch.matchedSegment, urlMatch.url, AUTOCOMPLETE_SOURCE_NAME, pages.size)
        }
    }

    override suspend fun deleteEverything() = synchronized(pages + pageMeta) {
        pages.clear()
        pageMeta.clear()
    }

    override suspend fun deleteVisitsSince(since: Long) = synchronized(pages) {
        pages.entries.forEach {
            it.setValue(it.value.filterNot { visit -> visit.timestamp >= since }.toMutableList())
        }
        pages = pages.filter { it.value.isNotEmpty() } as HashMap<String, MutableList<Visit>>
    }

    override suspend fun deleteVisitsBetween(startTime: Long, endTime: Long) = synchronized(pages) {
        pages.entries.forEach {
            it.setValue(it.value.filterNot { visit ->
                visit.timestamp >= startTime && visit.timestamp <= endTime
            }.toMutableList())
        }
        pages = pages.filter { it.value.isNotEmpty() } as HashMap<String, MutableList<Visit>>
    }

    override suspend fun deleteVisitsFor(url: String) = synchronized(pages + pageMeta) {
        pages.remove(url)
        pageMeta.remove(url)
        Unit
    }

    override suspend fun deleteVisit(url: String, timestamp: Long) = synchronized(pages) {
        if (pages.containsKey(url)) {
            pages[url] = pages[url]!!.filter { it.timestamp != timestamp }.toMutableList()
        }
    }

    override suspend fun prune() {
        // Not applicable.
    }

    override suspend fun runMaintenance() {
        // Not applicable.
    }

    override fun cleanup() {
        // GC will take care of our internal data structures, so there's nothing to do here.
    }
}
