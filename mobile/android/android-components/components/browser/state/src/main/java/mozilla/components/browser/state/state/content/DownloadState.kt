/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state.content

import android.os.Environment
import android.os.Parcelable
import kotlinx.android.parcel.Parcelize
import java.util.UUID

/**
 * Value type that represents a download request.
 *
 * @property url The full url to the content that should be downloaded.
 * @property fileName A canonical filename for this download.
 * @property contentType Content type (MIME type) to indicate the media type of the download.
 * @property contentLength The file size reported by the server.
 * @property currentBytesCopied The number of current bytes copied.
 * @property status The current status of the download.
 * @property userAgent The user agent to be used for the download.
 * @property destinationDirectory The matching destination directory for this type of download.
 * @property filePath The file path the file was saved at.
 * @property referrerUrl The site that linked to this download.
 * @property skipConfirmation Whether or not the confirmation dialog should be shown before the download begins.
 * @property id The unique identifier of this download.
 * @property createdTime A timestamp when the download was created.
 * @
 */
@Suppress("Deprecation")
@Parcelize
data class DownloadState(
    val url: String,
    val fileName: String? = null,
    val contentType: String? = null,
    val contentLength: Long? = null,
    val currentBytesCopied: Long = 0,
    val status: Status = Status.INITIATED,
    val userAgent: String? = null,
    val destinationDirectory: String = Environment.DIRECTORY_DOWNLOADS,
    val referrerUrl: String? = null,
    val skipConfirmation: Boolean = false,
    val id: String = UUID.randomUUID().toString(),
    val sessionId: String? = null,
    val createdTime: Long = System.currentTimeMillis()
) : Parcelable {
    val filePath: String get() =
        Environment.getExternalStoragePublicDirectory(destinationDirectory).path + "/" + fileName

    /**
     * Status that represents every state that a download can be in.
     */
    @Suppress("MagicNumber")
    enum class Status(val id: Int) {
        /**
         * Indicates that the download is in the first state after creation but not yet [DOWNLOADING].
         */
        INITIATED(1),
        /**
         * Indicates that an [INITIATED] download is now actively being downloaded.
         */
        DOWNLOADING(2),
        /**
         * Indicates that the download that has been [DOWNLOADING] has been paused.
         */
        PAUSED(3),
        /**
         * Indicates that the download that has been [DOWNLOADING] has been cancelled.
         */
        CANCELLED(4),
        /**
         * Indicates that the download that has been [DOWNLOADING] has moved to failed because
         * something unexpected has happened.
         */
        FAILED(5),
        /**
         * Indicates that the [DOWNLOADING] download has been completed.
         */
        COMPLETED(6)
    }
}
