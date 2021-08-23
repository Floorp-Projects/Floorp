/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import androidx.paging.ItemKeyedDataSource
import org.mozilla.fenix.components.history.PagedHistoryProvider

class HistoryDataSource(
    private val historyProvider: PagedHistoryProvider
) : ItemKeyedDataSource<Int, History>() {

    // Because the pagination is not based off of the key
    // we want to start at 1, not 0 to be able to send the correct offset
    // to the `historyProvider.getHistory` call.
    override fun getKey(item: History): Int = item.id + 1

    override fun loadInitial(
        params: LoadInitialParams<Int>,
        callback: LoadInitialCallback<History>
    ) {
        historyProvider.getHistory(INITIAL_OFFSET, params.requestedLoadSize.toLong()) { history ->
            callback.onResult(history)
        }
    }

    override fun loadAfter(params: LoadParams<Int>, callback: LoadCallback<History>) {
        historyProvider.getHistory(params.key.toLong(), params.requestedLoadSize.toLong()) { history ->
            callback.onResult(history)
        }
    }

    override fun loadBefore(params: LoadParams<Int>, callback: LoadCallback<History>) { /* noop */ }

    companion object {
        private const val INITIAL_OFFSET = 0L
    }
}
