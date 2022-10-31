/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state.content

import android.os.Environment
import mozilla.components.concept.fetch.Response
import java.io.File
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
 * @property private Indicates if the download was created from a private session.
 * @property createdTime A timestamp when the download was created.
 * @property response A response object associated with this request, when provided can be
 * used instead of performing a manual a download.
 * @property notificationId Identifies the download notification in the status bar, if this
 * [DownloadState] has one otherwise null.
 */
@Suppress("Deprecation")
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
    val private: Boolean = false,
    val createdTime: Long = System.currentTimeMillis(),
    val response: Response? = null,
    val notificationId: Int? = null,
) {
    val filePath: String get() =
        Environment.getExternalStoragePublicDirectory(destinationDirectory).path + File.separatorChar + fileName

    val directoryPath: String get() = Environment.getExternalStoragePublicDirectory(destinationDirectory).path

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
        COMPLETED(6),
    }
}
