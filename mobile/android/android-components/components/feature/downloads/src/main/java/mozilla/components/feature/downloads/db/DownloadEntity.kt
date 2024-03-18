/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads.db

import androidx.room.ColumnInfo
import androidx.room.Entity
import androidx.room.PrimaryKey
import mozilla.components.browser.state.state.content.DownloadState

/**
 * Internal entity representing a download as it gets saved to the database.
 */
@Entity(tableName = "downloads")
internal data class DownloadEntity(
    @PrimaryKey
    @ColumnInfo(name = "id")
    var id: String,

    @ColumnInfo(name = "url")
    var url: String,

    @ColumnInfo(name = "file_name")
    var fileName: String?,

    @ColumnInfo(name = "content_type")
    var contentType: String?,

    @ColumnInfo(name = "content_length")
    var contentLength: Long?,

    @ColumnInfo(name = "status")
    var status: DownloadState.Status,

    @ColumnInfo(name = "destination_directory")
    var destinationDirectory: String,

    @ColumnInfo(name = "created_at")
    var createdAt: Long,

) {

    internal fun toDownloadState(): DownloadState {
        return DownloadState(
            url,
            fileName,
            contentType,
            contentLength,
            currentBytesCopied = 0,
            status = status,
            userAgent = null,
            destinationDirectory = destinationDirectory,
            referrerUrl = null,
            skipConfirmation = false,
            id = id,
            sessionId = null,
            createdTime = createdAt,
        )
    }
}

internal fun DownloadState.toDownloadEntity(): DownloadEntity {
    /**
     * Data URLs cause problems when restoring the values from the db,
     * as the string could be so long that it could break the maximum allowed size for a cursor,
     * causing SQLiteBIobTooBigException when restoring downloads from the DB.
     */
    val isDataURL = url.startsWith("data:")
    val sanitizedURL = if (isDataURL) {
        ""
    } else {
        url
    }

    return DownloadEntity(
        id,
        sanitizedURL,
        fileName,
        contentType,
        contentLength,
        status = status,
        destinationDirectory = destinationDirectory,
        createdAt = createdTime,
    )
}
