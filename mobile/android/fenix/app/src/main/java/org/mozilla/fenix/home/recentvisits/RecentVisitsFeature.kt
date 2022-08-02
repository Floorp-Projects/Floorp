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
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItem.RecentHistoryHighlight
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItemInternal.HistoryHighlightInternal

@VisibleForTesting internal const val MAX_RESULTS_TOTAL = 9
@VisibleForTesting internal const val MIN_VIEW_TIME_OF_HIGHLIGHT = 10.0
@VisibleForTesting internal const val MIN_FREQUENCY_OF_HIGHLIGHT = 4.0

/**
 * View-bound feature that retrieves a list of [HistoryHighlight]s and [HistoryMetadata] items
 * which will be mapped to [RecentlyVisitedItem]s and then dispatched to [AppStore]
 * to be displayed on the home screen.
 *
 * @param appStore The [AppStore] that holds the state of the [HomeFragment].
 * @param historyMetadataStorage The storage that manages [HistoryMetadata].
 * @param historyHighlightsStorage The storage that manages [PlacesHistoryStorage].
 * @param scope The [CoroutineScope] used for IO operations related to querying history
 * and then for dispatching updates.
 * @param ioDispatcher The [CoroutineDispatcher] for performing read/write operations.
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
                    MAX_RESULTS_TOTAL
                )
            }

            val allHistoryMetadata = async {
                historyMetadataStorage.getHistoryMetadataSince(Long.MIN_VALUE)
            }

            val historyHighlights = getHistoryHighlights(highlights.await(), allHistoryMetadata.await())

            updateState(historyHighlights)
        }
    }

    @VisibleForTesting
    internal fun updateState(
        historyHighlights: List<HistoryHighlightInternal>,
    ) {
        appStore.dispatch(
            AppAction.RecentHistoryChange(
                getCombinedHistory(historyHighlights)
            )
        )
    }

    /**
     * Get up to [MAX_RESULTS_TOTAL] items if available of history highlights.
     * Maps the internal highlights and search groups to the final objects to be returned.
     * Items will be sorted by their last accessed date so that the most recent will be first.
     *
     * @param historyHighlights List of history highlights. Can be empty.
     *
     * @return [RecentlyVisitedItem] list representing the data expected by clients of this feature.
     */
    @VisibleForTesting
    internal fun getCombinedHistory(
        historyHighlights: List<HistoryHighlightInternal>,
    ): List<RecentlyVisitedItem> {
        return historyHighlights
            .sortedByDescending { it.lastAccessedTime }
            .take(MAX_RESULTS_TOTAL)
            .map {
                RecentHistoryHighlight(
                    title = if (it.historyHighlight.title.isNullOrBlank()) {
                        it.historyHighlight.url
                    } else {
                        it.historyHighlight.title!!
                    },
                    url = it.historyHighlight.url
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
        metadata: List<HistoryMetadata>
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
                    ?: 0
            )
        }
    }

    override fun stop() {
        job?.cancel()
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
        override val lastAccessedTime: Long
    ) : RecentlyVisitedItemInternal()
}
