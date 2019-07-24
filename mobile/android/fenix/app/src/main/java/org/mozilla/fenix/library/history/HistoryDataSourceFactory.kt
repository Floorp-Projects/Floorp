package org.mozilla.fenix.library.history

import androidx.lifecycle.MutableLiveData
import androidx.paging.DataSource
import org.mozilla.fenix.components.history.PagedHistoryProvider

class HistoryDataSourceFactory(
    private val historyProvider: PagedHistoryProvider
) : DataSource.Factory<Int, HistoryItem>() {

    val datasourceLiveData = MutableLiveData<HistoryDataSource>()

    override fun create(): DataSource<Int, HistoryItem> {
        val datasource = HistoryDataSource(historyProvider)
        datasourceLiveData.postValue(datasource)
        return datasource
    }
}
