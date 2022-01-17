/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import androidx.annotation.VisibleForTesting
import androidx.paging.ItemKeyedDataSource
import org.mozilla.fenix.components.history.HistoryDB
import org.mozilla.fenix.components.history.PagedHistoryProvider

class HistoryDataSource(
    private val historyProvider: PagedHistoryProvider
) : ItemKeyedDataSource<Int, History>() {

    // Because the pagination is not based off of the key
    // we want to start at 1, not 0 to be able to send the correct offset
    // to the `historyProvider.getHistory` call.
    override fun getKey(item: History): Int = item.position + 1

    override fun loadInitial(
        params: LoadInitialParams<Int>,
        callback: LoadInitialCallback<History>
    ) {
        historyProvider.getHistory(INITIAL_OFFSET, params.requestedLoadSize) { history ->
            callback.onResult(history.positionWithOffset(INITIAL_OFFSET))
        }
    }

    override fun loadAfter(params: LoadParams<Int>, callback: LoadCallback<History>) {
        historyProvider.getHistory(params.key, params.requestedLoadSize) { history ->
            callback.onResult(history.positionWithOffset(params.key))
        }
    }

    override fun loadBefore(params: LoadParams<Int>, callback: LoadCallback<History>) { /* noop */ }

    companion object {
        internal const val INITIAL_OFFSET = 0
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
