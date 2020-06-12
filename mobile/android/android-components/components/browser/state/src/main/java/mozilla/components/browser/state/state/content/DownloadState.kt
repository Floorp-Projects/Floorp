/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state.content

import android.os.Environment
import android.os.Parcelable
import kotlinx.android.parcel.Parcelize
import kotlin.random.Random

/**
 * Value type that represents a download request.
 *
 * @property url The full url to the content that should be downloaded.
 * @property fileName A canonical filename for this download.
 * @property contentType Content type (MIME type) to indicate the media type of the download.
 * @property contentLength The file size reported by the server.
 * @property userAgent The user agent to be used for the download.
 * @property destinationDirectory The matching destination directory for this type of download.
 * @property filePath The file path the file was saved at.
 * @property referrerUrl The site that linked to this download.
 * @property skipConfirmation Whether or not the confirmation dialog should be shown before the download begins.
 * @property id The unique identifier of this download.
 * @property sessionId Identifier of the session that spawned the download.
 * @
 */
@Suppress("Deprecation")
@Parcelize
data class DownloadState(
    val url: String,
    val fileName: String? = null,
    val contentType: String? = null,
    val contentLength: Long? = null,
    val userAgent: String? = null,
    val destinationDirectory: String = Environment.DIRECTORY_DOWNLOADS,
    val referrerUrl: String? = null,
    val skipConfirmation: Boolean = false,
    val id: Long = Random.nextLong(),
    val sessionId: String? = null
) : Parcelable {
    val filePath: String get() =
        Environment.getExternalStoragePublicDirectory(destinationDirectory).path + "/" + fileName
}
