/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.history

import androidx.annotation.VisibleForTesting
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.concept.storage.VisitInfo
import mozilla.components.concept.storage.VisitType
import mozilla.components.support.ktx.kotlin.tryGetHostFromUrl
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.library.history.History
import org.mozilla.fenix.library.history.toHistoryMetadata
import org.mozilla.fenix.perf.runBlockingIncrement
import kotlin.math.abs

private const val BUFFER_TIME = 15000 /* 15 seconds in ms */

/**
 * An Interface for providing a paginated list of [History].
 */
interface PagedHistoryProvider {
    /**
     * Gets a list of [History].
     *
     * @param offset How much to offset the list by
     * @param numberOfItems How many items to fetch
     * @param onComplete A callback that returns the list of [History]
     */
    fun getHistory(offset: Long, numberOfItems: Long, onComplete: (List<History>) -> Unit)
}

/**
 * @param historyStorage
 */
class DefaultPagedHistoryProvider(
    private val historyStorage: PlacesHistoryStorage,
    private val showHistorySearchGroups: Boolean = FeatureFlags.showHistorySearchGroups,
) : PagedHistoryProvider {
    /**
     * Types of visits we currently do not display in the History UI.
     */
    private val excludedVisitTypes = listOf(
        VisitType.NOT_A_VISIT,
        VisitType.DOWNLOAD,
        VisitType.REDIRECT_PERMANENT,
        VisitType.REDIRECT_TEMPORARY,
        VisitType.RELOAD,
        VisitType.EMBED,
        VisitType.FRAMED_LINK,
    )

    /**
     * All types of visits that aren't redirects. This is used for fetching only redirecting visits
     * from the store so that we can filter them out.
     */
    private val notRedirectTypes = VisitType.values().filterNot {
        it == VisitType.REDIRECT_PERMANENT || it == VisitType.REDIRECT_TEMPORARY
    }

    @Volatile private var historyGroups: List<History.Group>? = null

    @Suppress("LongMethod")
    override fun getHistory(
        offset: Long,
        numberOfItems: Long,
        onComplete: (List<History>) -> Unit,
    ) {
        // A PagedList DataSource runs on a background thread automatically.
        // If we run this in our own coroutineScope it breaks the PagedList
        runBlockingIncrement {
            val history: List<History>

            if (showHistorySearchGroups) {
                // We need to re-fetch all the history metadata if the offset resets back at 0
                // in the case of a pull to refresh.
                if (historyGroups == null || offset == 0L) {
                    historyGroups = historyStorage.getHistoryMetadataSince(Long.MIN_VALUE)
                        .sortedByDescending { it.createdAt }
                        .filter { it.key.searchTerm != null }
                        .groupBy { it.key.searchTerm!! }
                        .map { (searchTerm, items) ->
                            History.Group(
                                id = items.first().createdAt.toInt(),
                                title = searchTerm,
                                visitedAt = items.first().createdAt,
                                items = items.map { it.toHistoryMetadata() }
                            )
                        }
                }

                history = getHistoryAndSearchGroups(offset, numberOfItems)
            } else {
                history = historyStorage
                    .getVisitsPaginated(
                        offset,
                        numberOfItems,
                        excludeTypes = excludedVisitTypes
                    )
                    .mapIndexed(transformVisitInfoToHistoryItem(offset.toInt()))
            }

            onComplete(history)
        }
    }

    /**
     * Removes [group] and any corresponding history visits.
     */
    suspend fun deleteHistoryGroup(group: History.Group) {
        for (historyMetadata in group.items) {
            getMatchingHistory(historyMetadata)?.let {
                historyStorage.deleteVisit(
                    url = it.url,
                    timestamp = it.visitTime
                )
            }
        }

        historyStorage.deleteHistoryMetadata(
            searchTerm = group.title
        )

        // Force a re-fetch of the groups next time we go through #getHistory.
        historyGroups = null
    }

    /**
     * Returns the [History.Regular] corresponding to the given [History.Metadata] item.
     */
    private suspend fun getMatchingHistory(historyMetadata: History.Metadata): VisitInfo? {
        val history = historyStorage.getDetailedVisits(
            start = historyMetadata.visitedAt - BUFFER_TIME,
            end = historyMetadata.visitedAt + BUFFER_TIME,
            excludeTypes = excludedVisitTypes
        )
        return history
            .filter { it.url == historyMetadata.url }
            .minByOrNull { abs(historyMetadata.visitedAt - it.visitTime) }
    }

    @Suppress("MagicNumber")
    private suspend fun getHistoryAndSearchGroups(
        offset: Long,
        numberOfItems: Long,
    ): List<History> {
        val result = mutableListOf<History>()
        val history: List<History.Regular> = historyStorage
            .getVisitsPaginated(
                offset,
                numberOfItems,
                excludeTypes = excludedVisitTypes
            )
            .mapIndexed(transformVisitInfoToHistoryItem(offset.toInt()))

        // We'll use this list to filter out redirects from metadata groups below.
        val redirectsInThePage = if (history.isNotEmpty()) {
            historyStorage.getDetailedVisits(
                start = history.last().visitedAt,
                end = history.first().visitedAt,
                excludeTypes = notRedirectTypes
            ).map { it.url }
        } else {
            // Edge-case this doesn't cover: if we only had redirects in the current page,
            // we'd end up with an empty 'history' list since the redirects would have been
            // filtered out above. One possible solution would be to look at redirects in all of
            // history, but that's potentially quite expensive on large profiles, and introduces
            // other problems (e.g. pages that were redirects a month ago may not be redirects today).
            emptyList()
        }

        // History metadata items are recorded after their associated visited info, we add an
        // additional buffer time to the most recent visit to account for a history group
        // appearing as the most recent item.
        val visitedAtBuffer = if (offset == 0L) BUFFER_TIME else 0

        // Get the history groups that fit within the range of visited times in the current history
        // items.
        val historyGroupsInOffset = if (history.isNotEmpty()) {
            historyGroups?.filter {
                it.items.any { item ->
                    (history.last().visitedAt - visitedAtBuffer) <= item.visitedAt &&
                        item.visitedAt <= (history.first().visitedAt + visitedAtBuffer)
                }
            } ?: emptyList()
        } else {
            emptyList()
        }
        val historyMetadata = historyGroupsInOffset.flatMap { it.items }

        // Add all history items that are not in a group filtering out any matches with a history
        // metadata item.
        result.addAll(history.filter { item -> historyMetadata.find { it.url == item.url } == null })

        // Filter history metadata items with no view time and dedupe by url.
        // Note that distinctBy is sufficient here as it keeps the order of the source
        // collection, and we're only sorting by visitedAt (=updatedAt) currently.
        // If we needed the view time we'd have to aggregate it for entries with the same
        // url, but we don't have a use case for this currently in the history view.
        result.addAll(
            historyGroupsInOffset.map { group ->
                group.copy(items = group.items.distinctBy { it.url }.filterNot { redirectsInThePage.contains(it.url) })
            }
        )

        return result.removeConsecutiveDuplicates()
            .sortedByDescending { it.visitedAt }
    }

    private fun transformVisitInfoToHistoryItem(offset: Int): (id: Int, visit: VisitInfo) -> History.Regular {
        return { id, visit ->
            val title = visit.title
                ?.takeIf(String::isNotEmpty)
                ?: visit.url.tryGetHostFromUrl()

            History.Regular(
                id = offset + id,
                title = title,
                url = visit.url,
                visitedAt = visit.visitTime
            )
        }
    }
}

@VisibleForTesting
internal fun List<History>.removeConsecutiveDuplicates(): List<History> {
    var previousURL = ""
    return filter {
        var isNotDuplicate = true
        previousURL = if (it is History.Regular) {
            isNotDuplicate = it.url != previousURL
            it.url
        } else {
            ""
        }
        isNotDuplicate
    }
}
