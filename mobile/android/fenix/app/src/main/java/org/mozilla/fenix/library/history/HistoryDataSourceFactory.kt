/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import androidx.lifecycle.MutableLiveData
import androidx.paging.DataSource
import org.mozilla.fenix.components.history.PagedHistoryProvider

class HistoryDataSourceFactory(
    private val historyProvider: PagedHistoryProvider
) : DataSource.Factory<Int, HistoryItem>() {
    val datasource = MutableLiveData<HistoryDataSource>()

    override fun create(): DataSource<Int, HistoryItem> {
        val datasource = HistoryDataSource(historyProvider)
        this.datasource.postValue(datasource)
        return datasource
    }
}
