/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import androidx.paging.Pager
import androidx.paging.PagingConfig
import androidx.paging.PagingData
import kotlinx.coroutines.flow.Flow
import org.mozilla.fenix.components.history.PagedHistoryProvider

class HistoryViewModel(historyProvider: PagedHistoryProvider) : ViewModel() {
    var userHasHistory = MutableLiveData(true)
    var history: Flow<PagingData<History>> = Pager(
        PagingConfig(PAGE_SIZE),
        null
    ) {
        HistoryDataSource(
            historyProvider = historyProvider
        )
    }.flow

    companion object {
        private const val PAGE_SIZE = 25
    }
}
