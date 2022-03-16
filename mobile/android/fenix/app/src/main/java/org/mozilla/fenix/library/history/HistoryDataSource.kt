/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import androidx.annotation.VisibleForTesting
import androidx.paging.PagingSource
import androidx.paging.PagingState
import org.mozilla.fenix.components.history.HistoryDB
import org.mozilla.fenix.components.history.PagedHistoryProvider

class HistoryDataSource(
    private val historyProvider: PagedHistoryProvider
) : PagingSource<Int, History>() {

    // having any value but null creates visual glitches in case or swipe to refresh and immediate
    // scroll down
    override fun getRefreshKey(state: PagingState<Int, History>): Int? = null

    // params.key is expected to be null for the initial load or a refresh
    override suspend fun load(params: LoadParams<Int>): LoadResult<Int, History> {
        val offset = params.key ?: 0
        val historyItems = historyProvider.getHistory(offset, params.loadSize).run {
            positionWithOffset(offset)
        }
        val nextOffset = if (historyItems.isEmpty()) {
            null
        } else {
            (offset + historyItems.size) + 1
        }
        // prevKey is needed in case load would work upwards, so passing null is fine
        return LoadResult.Page(
            data = historyItems,
            prevKey = null,
            nextKey = nextOffset
        )
    }
}

@VisibleForTesting
internal fun List<HistoryDB>.positionWithOffset(offset: Int): List<History> {
    return this.foldIndexed(listOf()) { index, prev, item ->
        // Only offset once while folding, so that we don't accumulate the offset for each element.
        val itemOffset = if (index == 0) {
            offset
        } else {
            0
        }
        val previousPosition = prev.lastOrNull()?.position ?: 0
        when (item) {
            is HistoryDB.Group -> {
                // XXX considering an empty group to have a non-zero offset is the obvious
                // limitation of the current approach, and indicates that we're conflating
                // two concepts here - position of an element for the sake of a RecyclerView,
                // and an offset for the sake of our history pagination API.
                val groupOffset = if (item.items.isEmpty()) {
                    1
                } else {
                    item.items.size
                }
                prev + item.positioned(position = previousPosition + itemOffset + groupOffset)
            }
            is HistoryDB.Metadata -> {
                prev + item.positioned(previousPosition + itemOffset + 1)
            }
            is HistoryDB.Regular -> {
                prev + item.positioned(previousPosition + itemOffset + 1)
            }
        }
    }
}

private fun HistoryDB.Group.positioned(position: Int): History.Group {
    return History.Group(
        position = position,
        items = this.items.mapIndexed { index, item -> item.positioned(index) },
        title = this.title,
        visitedAt = this.visitedAt,
        historyTimeGroup = this.historyTimeGroup,
    )
}

private fun HistoryDB.Metadata.positioned(position: Int): History.Metadata {
    return History.Metadata(
        position = position,
        historyMetadataKey = this.historyMetadataKey,
        title = this.title,
        totalViewTime = this.totalViewTime,
        url = this.url,
        visitedAt = this.visitedAt,
        historyTimeGroup = this.historyTimeGroup,
    )
}

private fun HistoryDB.Regular.positioned(position: Int): History.Regular {
    return History.Regular(
        position = position,
        title = this.title,
        url = this.url,
        visitedAt = this.visitedAt,
        historyTimeGroup = this.historyTimeGroup,
    )
}
