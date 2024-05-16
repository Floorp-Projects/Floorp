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
 * @property directoryPath The full path to the directory where the file should be saved.
 * @property filePath The file path the file was saved at.
 * @property referrerUrl The site that linked to this download.
 * @property skipConfirmation Whether or not the confirmation dialog should be shown before the download begins.
 * @property openInApp Whether or not the file associated with this download should be opened in a
 * third party app after downloaded successfully.
 * @property id The unique identifier of this download.
 * @property private Indicates if the download was created from a private session.
 * @property createdTime A timestamp when the download was created.
 * @property response A response object associated with this request, when provided can be
 * used instead of performing a manual a download.
 * @property notificationId Identifies the download notification in the status bar, if this
 * [DownloadState] has one otherwise null.
 */
data class DownloadState(
    val url: String,
    val fileName: String? = null,
    val contentType: String? = null,
    val contentLength: Long? = null,
    val currentBytesCopied: Long = 0,
    val status: Status = Status.INITIATED,
    val userAgent: String? = null,
    val destinationDirectory: String = Environment.DIRECTORY_DOWNLOADS,
    val directoryPath: String = Environment.getExternalStoragePublicDirectory(destinationDirectory).path,
    val referrerUrl: String? = null,
    val skipConfirmation: Boolean = false,
    val openInApp: Boolean = false,
    val id: String = UUID.randomUUID().toString(),
    val sessionId: String? = null,
    val private: Boolean = false,
    val createdTime: Long = System.currentTimeMillis(),
    val response: Response? = null,
    val notificationId: Int? = null,
) {
    val filePath: String
        get() = directoryPath + File.separatorChar + fileName

    /**
     * Status that represents every state that a download can be in.
     */
    enum class Status(val id: Int) {
        /**
         * Indicates that the download is in the first state after creation but not yet [DOWNLOADING].
         */
        INITIATED(INITIATED_ID),

        /**
         * Indicates that an [INITIATED] download is now actively being downloaded.
         */
        DOWNLOADING(DOWNLOADING_ID),

        /**
         * Indicates that the download that has been [DOWNLOADING] has been paused.
         */
        PAUSED(PAUSED_ID),

        /**
         * Indicates that the download that has been [DOWNLOADING] has been cancelled.
         */
        CANCELLED(CANCELLED_ID),

        /**
         * Indicates that the download that has been [DOWNLOADING] has moved to failed because
         * something unexpected has happened.
         */
        FAILED(FAILED_ID),

        /**
         * Indicates that the [DOWNLOADING] download has been completed.
         */
        COMPLETED(COMPLETED_ID),
    }

    companion object {
        private const val INITIATED_ID = 1
        private const val DOWNLOADING_ID = 2
        private const val PAUSED_ID = 3
        private const val CANCELLED_ID = 4
        private const val FAILED_ID = 5
        private const val COMPLETED_ID = 6
    }
}
