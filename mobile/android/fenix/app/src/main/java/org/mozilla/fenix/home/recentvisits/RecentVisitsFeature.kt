/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentvisits

import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.concept.storage.HistoryHighlight
import mozilla.components.concept.storage.HistoryHighlightWeights
import mozilla.components.concept.storage.HistoryMetadata
import mozilla.components.concept.storage.HistoryMetadataStorage
import mozilla.components.support.base.feature.LifecycleAwareFeature
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.home.HomeFragment
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItem.RecentHistoryGroup
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItem.RecentHistoryHighlight
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItemInternal.HistoryGroupInternal
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItemInternal.HistoryHighlightInternal
import org.mozilla.fenix.utils.Settings.Companion.SEARCH_GROUP_MINIMUM_SITES
import kotlin.math.max

@VisibleForTesting internal const val MAX_RESULTS_TOTAL = 9

@VisibleForTesting internal const val MIN_VIEW_TIME_OF_HIGHLIGHT = 10.0

@VisibleForTesting internal const val MIN_FREQUENCY_OF_HIGHLIGHT = 4.0

/**
 * View-bound feature that retrieves a list of [HistoryHighlight]s and [HistoryMetadata] items
 * which will be mapped to [RecentlyVisitedItem]s and then dispatched to [AppStore]
 * to be displayed on the home screen.
 *
 * @property appStore The [AppStore] that holds the state of the [HomeFragment].
 * @property historyMetadataStorage The storage that manages [HistoryMetadata].
 * @property historyHighlightsStorage The storage that manages [PlacesHistoryStorage].
 * @property scope The [CoroutineScope] used for IO operations related to querying history
 * and then for dispatching updates.
 * @property ioDispatcher The [CoroutineDispatcher] for performing read/write operations.
 */
class RecentVisitsFeature(
    private val appStore: AppStore,
    private val historyMetadataStorage: HistoryMetadataStorage,
    private val historyHighlightsStorage: Lazy<PlacesHistoryStorage>,
    private val scope: CoroutineScope,
    private val ioDispatcher: CoroutineDispatcher = Dispatchers.IO,
) : LifecycleAwareFeature {

    private var job: Job? = null

    override fun start() {
        job = scope.launch(ioDispatcher) {
            val highlights = async {
                historyHighlightsStorage.value.getHistoryHighlights(
                    HistoryHighlightWeights(MIN_VIEW_TIME_OF_HIGHLIGHT, MIN_FREQUENCY_OF_HIGHLIGHT),
                    MAX_RESULTS_TOTAL,
                )
            }

            val allHistoryMetadata = async {
                historyMetadataStorage.getHistoryMetadataSince(Long.MIN_VALUE)
            }

            val historyHighlights = getHistoryHighlights(highlights.await(), allHistoryMetadata.await())
            val historyGroups = getHistorySearchGroups(allHistoryMetadata.await())

            updateState(historyHighlights, historyGroups)
        }
    }

    @VisibleForTesting
    internal fun updateState(
        historyHighlights: List<HistoryHighlightInternal>,
        historyGroups: List<HistoryGroupInternal>,
    ) {
        appStore.dispatch(
            AppAction.RecentHistoryChange(
                getCombinedHistory(historyHighlights, historyGroups),
            ),
        )
    }

    /**
     * Get up to [MAX_RESULTS_TOTAL] items if available as an even split of history highlights and history groups.
     * If more items then needed are available then highlights will be more by one.
     *
     * @param historyHighlights List of history highlights. Can be empty.
     * @param historyGroups List of history groups. Can be empty.
     *
     * @return [RecentlyVisitedItem] list representing the data expected by clients of this feature.
     */
    @VisibleForTesting
    internal fun getCombinedHistory(
        historyHighlights: List<HistoryHighlightInternal>,
        historyGroups: List<HistoryGroupInternal>,
    ): List<RecentlyVisitedItem> {
        // Cleanup highlights now to avoid counting them below and then removing the ones found in groups.
        val distinctHighlights = historyHighlights
            .removeHighlightsAlreadyInGroups(historyGroups)

        val totalItemsCount = distinctHighlights.size + historyGroups.size

        return if (totalItemsCount <= MAX_RESULTS_TOTAL) {
            getSortedHistory(
                distinctHighlights.sortedByDescending { it.lastAccessedTime },
                historyGroups.sortedByDescending { it.lastAccessedTime },
            )
        } else {
            var groupsCount = 0
            var highlightCount = 0
            while ((highlightCount + groupsCount) < MAX_RESULTS_TOTAL) {
                if ((highlightCount + groupsCount) < MAX_RESULTS_TOTAL &&
                    distinctHighlights.getOrNull(highlightCount) != null
                ) {
                    highlightCount += 1
                }

                if ((highlightCount + groupsCount) < MAX_RESULTS_TOTAL &&
                    historyGroups.getOrNull(groupsCount) != null
                ) {
                    groupsCount += 1
                }
            }

            getSortedHistory(
                distinctHighlights
                    .sortedByDescending { it.lastAccessedTime }
                    .take(highlightCount),
                historyGroups
                    .sortedByDescending { it.lastAccessedTime }
                    .take(groupsCount),
            )
        }
    }

    /**
     * Perform an in-memory mapping of a history highlight to metadata records to compute its last access time.
     *
     * - If a `highlight` cannot be mapped to a corresponding `metadata` record, its lastAccessTime will be set to 0.
     * - If a `highlight` maps to multiple metadata records, its lastAccessTime will be set to the most recently
     * updated record.
     *
     * @param highlights [HistoryHighlight] list for which to get the last accessed time.
     * @param metadata [HistoryMetadata] list expected to contain the details for all [highlights].
     *
     * @return The [highlights] with a computed last accessed time.
     */
    @VisibleForTesting
    internal fun getHistoryHighlights(
        highlights: List<HistoryHighlight>,
        metadata: List<HistoryMetadata>,
    ): List<HistoryHighlightInternal> {
        val highlightsUrls = highlights.map { it.url }
        val highlightsLastUpdatedTime = metadata
            .filter { highlightsUrls.contains(it.key.url) }
            .groupBy { it.key.url }
            .map { (url, data) ->
                url to data.maxByOrNull { it.updatedAt }!!
            }

        return highlights.map {
            HistoryHighlightInternal(
                historyHighlight = it,
                lastAccessedTime = highlightsLastUpdatedTime
                    .firstOrNull { (url, _) -> url == it.url }?.second?.updatedAt
                    ?: 0,
            )
        }
    }

    /**
     * Group all urls accessed following a particular search.
     * Automatically dedupes identical urls and adds each url's view time to the group's total.
     *
     * @param metadata List of history visits.
     *
     * @return List of user searches and all urls accessed from those.
     */
    @VisibleForTesting
    internal fun getHistorySearchGroups(
        metadata: List<HistoryMetadata>,
    ): List<HistoryGroupInternal> {
        return metadata
            .filter { it.totalViewTime > 0 && it.key.searchTerm != null }
            .groupBy { it.key.searchTerm!! }
            .mapValues { group ->
                // Within a group, we dedupe entries based on their url so we don't display
                // a page multiple times in the same group, and we sum up the total view time
                // of deduped entries while making sure to keep the latest updatedAt value.
                val metadataInGroup = group.value
                val metadataUrlGroups = metadataInGroup.groupBy { metadata -> metadata.key.url }
                metadataUrlGroups.map { metadata ->
                    metadata.value.reduce { acc, elem ->
                        acc.copy(
                            totalViewTime = acc.totalViewTime + elem.totalViewTime,
                            updatedAt = max(acc.updatedAt, elem.updatedAt),
                        )
                    }
                }
            }
            .map {
                HistoryGroupInternal(
                    groupName = it.key,
                    groupItems = it.value,
                )
            }
            .filter {
                it.groupItems.size >= SEARCH_GROUP_MINIMUM_SITES
            }
    }

    /**
     * Maps the internal highlights and search groups to the final objects to be returned.
     * Items will be sorted by their last accessed date so that the most recent will be first.
     */
    @VisibleForTesting
    internal fun getSortedHistory(
        historyHighlights: List<HistoryHighlightInternal>,
        historyGroups: List<HistoryGroupInternal>,
    ): List<RecentlyVisitedItem> {
        return (historyHighlights + historyGroups)
            .sortedByDescending { it.lastAccessedTime }
            .map {
                when (it) {
                    is HistoryHighlightInternal -> RecentHistoryHighlight(
                        title = if (it.historyHighlight.title.isNullOrBlank()) {
                            it.historyHighlight.url
                        } else {
                            it.historyHighlight.title!!
                        },
                        url = it.historyHighlight.url,
                    )
                    is HistoryGroupInternal -> RecentHistoryGroup(
                        title = it.groupName,
                        historyMetadata = it.groupItems,
                    )
                }
            }
    }

    override fun stop() {
        job?.cancel()
    }
}

/**
 * Filter out highlights that are already part of a history group.
 */
@VisibleForTesting
internal fun List<HistoryHighlightInternal>.removeHighlightsAlreadyInGroups(
    historyMetadata: List<HistoryGroupInternal>,
): List<HistoryHighlightInternal> {
    return filterNot { highlight ->
        historyMetadata.any {
            it.groupItems.any {
                it.key.url == highlight.historyHighlight.url
            }
        }
    }
}

@VisibleForTesting
internal sealed class RecentlyVisitedItemInternal {
    abstract val lastAccessedTime: Long

    /**
     * Temporary wrapper over a [HistoryHighlight] which adds a [lastAccessedTime] property used for sorting.
     */
    data class HistoryHighlightInternal(
        val historyHighlight: HistoryHighlight,
        override val lastAccessedTime: Long,
    ) : RecentlyVisitedItemInternal()

    /**
     * Temporary search group allowing for easier data manipulation.
     */
    data class HistoryGroupInternal(
        val groupName: String,
        val groupItems: List<HistoryMetadata>,
        override val lastAccessedTime: Long = groupItems.maxOf { it.updatedAt },
    ) : RecentlyVisitedItemInternal()
}
