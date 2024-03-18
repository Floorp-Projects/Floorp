/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.paging.DataSource
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.feature.downloads.db.DownloadsDatabase
import mozilla.components.feature.downloads.db.toDownloadEntity

/**
 * A storage implementation for organizing download.
 */
class DownloadStorage(context: Context) {

    @VisibleForTesting
    internal var database: Lazy<DownloadsDatabase> = lazy { DownloadsDatabase.get(context) }

    private val downloadDao by lazy { database.value.downloadDao() }

    /**
     * Adds a new [download].
     */
    suspend fun add(download: DownloadState) {
        downloadDao.insert(download.toDownloadEntity())
    }

    /**
     * Returns a [Flow] list of all the [DownloadState] instances.
     */
    fun getDownloads(): Flow<List<DownloadState>> {
        return downloadDao.getDownloads().map { list ->
            list.map { entity -> entity.toDownloadState() }
        }
    }

    /**
     * Returns a [List] of all the [DownloadState] instances.
     */
    suspend fun getDownloadsList(): List<DownloadState> {
        return downloadDao.getDownloadsList().map { entity ->
            entity.toDownloadState()
        }
    }

    /**
     * Returns all saved [DownloadState] instances as a [DataSource.Factory].
     */
    fun getDownloadsPaged(): DataSource.Factory<Int, DownloadState> = downloadDao
        .getDownloadsPaged()
        .map { entity ->
            entity.toDownloadState()
        }

    /**
     * Removes the given [download].
     */
    suspend fun remove(download: DownloadState) {
        downloadDao.delete(download.toDownloadEntity())
    }

    /**
     * Update the given [download].
     */
    suspend fun update(download: DownloadState) {
        downloadDao.update(download.toDownloadEntity())
    }

    /**
     * Removes all the downloads.
     */
    suspend fun removeAllDownloads() {
        downloadDao.deleteAllDownloads()
    }

    companion object {
        /**
         * Takes two [DownloadState] objects and the determine if they are the same, be aware this
         * only takes into considerations fields that are being stored,
         * not all the field on [DownloadState] are stored.
         */
        fun isSameDownload(first: DownloadState, second: DownloadState): Boolean {
            return first.id == second.id &&
                first.fileName == second.fileName &&
                first.url == second.url &&
                first.contentType == second.contentType &&
                first.contentLength == second.contentLength &&
                first.status == second.status &&
                first.destinationDirectory == second.destinationDirectory &&
                first.createdTime == second.createdTime
        }
    }
}
