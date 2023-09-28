/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.history

import androidx.annotation.VisibleForTesting
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.concept.storage.HistoryMetadata
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.concept.storage.VisitInfo
import mozilla.components.concept.storage.VisitType
import mozilla.components.support.ktx.kotlin.tryGetHostFromUrl
import org.mozilla.fenix.library.history.History
import org.mozilla.fenix.library.history.HistoryItemTimeGroup
import org.mozilla.fenix.utils.Settings.Companion.SEARCH_GROUP_MINIMUM_SITES

private const val BUFFER_TIME = 15000 // 15 seconds in ms

/**
 * Class representing a history entry.
 * Contrast this with [History] that's the same, but with an assigned position, for pagination
 * and display purposes.
 */
sealed class HistoryDB {
    abstract val title: String
    abstract val visitedAt: Long
    abstract val selected: Boolean
    val historyTimeGroup: HistoryItemTimeGroup by lazy {
        HistoryItemTimeGroup.timeGroupForTimestamp(visitedAt)
    }

    data class Regular(
        override val title: String,
        val url: String,
        override val visitedAt: Long,
        override val selected: Boolean = false,
        val isRemote: Boolean = false,
    ) : HistoryDB()

    data class Metadata(
        override val title: String,
        val url: String,
        override val visitedAt: Long,
        val totalViewTime: Int,
        val historyMetadataKey: HistoryMetadataKey,
        override val selected: Boolean = false,
    ) : HistoryDB()

    data class Group(
        override val title: String,
        override val visitedAt: Long,
        val items: List<Metadata>,
        override val selected: Boolean = false,
    ) : HistoryDB()
}

private fun HistoryMetadata.toHistoryDBMetadata(): HistoryDB.Metadata {
    return HistoryDB.Metadata(
        title = title?.takeIf(String::isNotEmpty)
            ?: key.url.tryGetHostFromUrl(),
        url = key.url,
        visitedAt = createdAt,
        totalViewTime = totalViewTime,
        historyMetadataKey = key,
    )
}

/**
 * An Interface for providing a paginated list of [HistoryDB].
 */
interface PagedHistoryProvider {
    /**
     * Gets a list of [HistoryDB].
     *
     * @param offset How much to offset the list by
     * @param numberOfItems How many items to fetch
     * @return list of [HistoryDB]
     */
    suspend fun getHistory(offset: Int, numberOfItems: Int): List<HistoryDB>
}

/**
 * @property historyStorage An instance [PlacesHistoryStorage] that provides read/write methods for
 * history data.
 */
class DefaultPagedHistoryProvider(
    private val historyStorage: PlacesHistoryStorage,
) : PagedHistoryProvider {

    /**
     * Types of visits we currently do not display in the History UI.
     */
    private val excludedVisitTypes = listOf(
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

    @Volatile private var historyGroups: List<HistoryDB.Group>? = null

    override suspend fun getHistory(
        offset: Int,
        numberOfItems: Int,
    ): List<HistoryDB> {
        // We need to re-fetch all the history metadata if the offset resets back at 0
        // in the case of a pull to refresh.
        if (historyGroups == null || offset == 0) {
            historyGroups = historyStorage.getHistoryMetadataSince(Long.MIN_VALUE)
                .asSequence()
                .sortedByDescending { it.createdAt }
                .filter { it.key.searchTerm != null }
                .groupBy { it.key.searchTerm!! }
                .map { (searchTerm, items) ->
                    HistoryDB.Group(
                        title = searchTerm,
                        visitedAt = items.first().createdAt,
                        items = items.map { it.toHistoryDBMetadata() },
                    )
                }
                .filter {
                    it.items.size >= SEARCH_GROUP_MINIMUM_SITES
                }
                .toList()
        }

        return getHistoryAndSearchGroups(offset, numberOfItems)
    }

    /**
     * Removes [group] and any corresponding history visits.
     */
    suspend fun deleteMetadataSearchGroup(group: History.Group) {
        // The intention is to delete items from history for good.
        // Corresponding metadata items would also be removed,
        // because of ON DELETE CASCADE relation in DB schema.
        for (historyMetadata in group.items) {
            historyStorage.deleteVisitsFor(historyMetadata.url)
        }

        // Force a re-fetch of the groups next time we go through #getHistory.
        historyGroups = null
    }

    @Suppress("MagicNumber")
    private suspend fun getHistoryAndSearchGroups(
        offset: Int,
        numberOfItems: Int,
    ): List<HistoryDB> {
        val result = mutableListOf<HistoryDB>()
        var history: List<HistoryDB.Regular> = historyStorage
            .getVisitsPaginated(
                offset.toLong(),
                numberOfItems.toLong(),
                excludeTypes = excludedVisitTypes,
            )
            .map { transformVisitInfoToHistoryItem(it) }

        // We'll use this list to filter out redirects from metadata groups below.
        val redirectsInThePage = if (history.isNotEmpty()) {
            historyStorage.getDetailedVisits(
                start = history.last().visitedAt,
                end = history.first().visitedAt,
                excludeTypes = notRedirectTypes,
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
        val visitedAtBuffer = if (offset == 0) BUFFER_TIME else 0

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
        history = history.distinctBy { Pair(it.historyTimeGroup, it.url) }

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
            },
        )

        return result.sortedByDescending { it.visitedAt }
    }

    private fun transformVisitInfoToHistoryItem(visit: VisitInfo): HistoryDB.Regular {
        val title = visit.title
            ?.takeIf(String::isNotEmpty)
            ?: visit.url.tryGetHostFromUrl()

        return HistoryDB.Regular(
            title = title,
            url = visit.url,
            visitedAt = visit.visitTime,
            isRemote = visit.isRemote,
        )
    }
}

@VisibleForTesting
internal fun List<HistoryDB>.removeConsecutiveDuplicates(): List<HistoryDB> {
    var previousURL = ""
    return filter {
        var isNotDuplicate = true
        previousURL = if (it is HistoryDB.Regular) {
            isNotDuplicate = it.url != previousURL
            it.url
        } else {
            ""
        }
        isNotDuplicate
    }
}
