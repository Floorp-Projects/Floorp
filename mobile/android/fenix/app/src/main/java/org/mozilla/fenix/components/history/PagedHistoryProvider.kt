/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.history

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
                // We need to refetch all the history metadata if the offset resets back at 0
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
                        excludeTypes = listOf(
                            VisitType.NOT_A_VISIT,
                            VisitType.DOWNLOAD,
                            VisitType.REDIRECT_TEMPORARY,
                            VisitType.RELOAD,
                            VisitType.EMBED,
                            VisitType.FRAMED_LINK,
                            VisitType.REDIRECT_PERMANENT
                        )
                    )
                    .mapIndexed(transformVisitInfoToHistoryItem(offset.toInt()))
            }

            onComplete(history)
        }
    }

    /**
     * Returns the [History.Regular] corresponding to the given [History.Metadata] item.
     *
     * @param historyMetadata The [History.Metadata] to match.
     * @return the [History.Regular] corresponding to the given [History.Metadata] item or null.
     */
    suspend fun getMatchingHistory(historyMetadata: History.Metadata): VisitInfo? {
        val history = historyStorage.getDetailedVisits(
            start = historyMetadata.visitedAt - BUFFER_TIME,
            end = historyMetadata.visitedAt + BUFFER_TIME,
            excludeTypes = listOf(
                VisitType.NOT_A_VISIT,
                VisitType.DOWNLOAD,
                VisitType.REDIRECT_TEMPORARY,
                VisitType.RELOAD,
                VisitType.EMBED,
                VisitType.FRAMED_LINK,
                VisitType.REDIRECT_PERMANENT
            )
        )
        return history
            .filter { it.url == historyMetadata.url }
            .minByOrNull { abs(historyMetadata.visitedAt - it.visitTime) }
    }

    /**
     * Clears the history groups to refetch the most history metadata after any changes.
     */
    fun clearHistoryGroups() {
        historyGroups = null
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
                excludeTypes = listOf(
                    VisitType.NOT_A_VISIT,
                    VisitType.DOWNLOAD,
                    VisitType.REDIRECT_TEMPORARY,
                    VisitType.RELOAD,
                    VisitType.EMBED,
                    VisitType.FRAMED_LINK,
                    VisitType.REDIRECT_PERMANENT
                )
            )
            .mapIndexed(transformVisitInfoToHistoryItem(offset.toInt()))

        // History metadata items are recorded after their associated visited info, we add an
        // additional buffer time to the most recent visit to account for a history group
        // appearing as the most recent item.
        val visitedAtBuffer = if (offset == 0L) BUFFER_TIME else 0

        // Get the history groups that fit within the range of visited times in the current history
        // items.
        val historyGroupsInOffset = if (history.isNotEmpty()) {
            historyGroups?.filter {
                it.items.any { item ->
                    history.last().visitedAt <= item.visitedAt - visitedAtBuffer &&
                        item.visitedAt - visitedAtBuffer <= (history.first().visitedAt + visitedAtBuffer)
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
                group.copy(items = group.items.distinctBy { it.url })
            }
        )

        return result.sortedByDescending { it.visitedAt }
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
