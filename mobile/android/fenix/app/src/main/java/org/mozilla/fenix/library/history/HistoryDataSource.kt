/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import androidx.paging.ItemKeyedDataSource
import mozilla.components.concept.storage.VisitInfo
import org.mozilla.fenix.components.history.PagedHistoryProvider
import org.mozilla.fenix.ext.getHostFromUrl

class HistoryDataSource(
    private val historyProvider: PagedHistoryProvider
) : ItemKeyedDataSource<Int, HistoryItem>() {

    // Because the pagination is not based off of they key
    // we want to start at 1, not 0 to be able to send the correct offset
    // to the `historyProvider.getHistory` call.
    override fun getKey(item: HistoryItem): Int = item.id + 1

    override fun loadInitial(
        params: LoadInitialParams<Int>,
        callback: LoadInitialCallback<HistoryItem>
    ) {
        historyProvider.getHistory(INITIAL_OFFSET, params.requestedLoadSize.toLong()) { history ->
            val items = history.mapIndexed(transformVisitInfoToHistoryItem(INITIAL_OFFSET.toInt()))
            callback.onResult(items)
        }
    }

    override fun loadAfter(params: LoadParams<Int>, callback: LoadCallback<HistoryItem>) {
        historyProvider.getHistory(params.key.toLong(), params.requestedLoadSize.toLong()) { history ->
            val items = history.mapIndexed(transformVisitInfoToHistoryItem(params.key))
            callback.onResult(items)
        }
    }

    override fun loadBefore(params: LoadParams<Int>, callback: LoadCallback<HistoryItem>) { /* noop */ }

    companion object {
        private const val INITIAL_OFFSET = 0L

        fun transformVisitInfoToHistoryItem(offset: Int): (id: Int, visit: VisitInfo) -> HistoryItem {
            return { id, visit ->
                val title = visit.title
                    ?.takeIf(String::isNotEmpty)
                    ?: visit.url.getHostFromUrl()
                    ?: visit.url

                HistoryItem(offset + id, title, visit.url, visit.visitTime)
            }
        }
    }
}
