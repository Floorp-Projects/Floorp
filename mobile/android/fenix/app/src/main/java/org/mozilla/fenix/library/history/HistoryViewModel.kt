/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import androidx.lifecycle.LiveData
import androidx.lifecycle.ViewModel
import androidx.paging.PagedList
import androidx.paging.LivePagedListBuilder
import org.mozilla.fenix.components.history.PagedHistoryProvider

class HistoryViewModel(historyProvider: PagedHistoryProvider) : ViewModel() {
    var history: LiveData<PagedList<HistoryItem>>
    private val datasource: LiveData<HistoryDataSource>

    init {
        val historyDataSourceFactory = HistoryDataSourceFactory(historyProvider)
        datasource = historyDataSourceFactory.datasourceLiveData

        history = LivePagedListBuilder(historyDataSourceFactory, PAGE_SIZE).build()
    }

    fun invalidate() {
        datasource.value?.invalidate()
    }

    companion object {
        private const val PAGE_SIZE = 25
    }
}
