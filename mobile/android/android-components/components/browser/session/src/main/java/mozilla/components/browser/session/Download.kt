/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session

import android.os.Environment
import mozilla.components.browser.state.state.content.DownloadState
import java.util.UUID

/**
 * Value type that represents a Download.
 *
 * @property url The full url to the content that should be downloaded.
 * @property fileName A canonical filename for this download.
 * @property contentType Content type (MIME type) to indicate the media type of the download.
 * @property contentLength The file size reported by the server.
 * @property userAgent The user agent to be used for the download.
 * @property destinationDirectory The matching destination directory for this type of download.
 * @property referrerUrl The site that linked to this download.
 * @property id a unique ID of this download.
 */
data class Download(
    val url: String,
    val fileName: String? = null,
    val contentType: String? = null,
    val contentLength: Long? = null,
    val userAgent: String? = null,
    val destinationDirectory: String = Environment.DIRECTORY_DOWNLOADS,
    val referrerUrl: String? = null,
    val id: String = UUID.randomUUID().toString()
)

internal fun Download.toDownloadState() = DownloadState(
    url,
    fileName,
    contentType,
    contentLength,
    userAgent,
    destinationDirectory,
    referrerUrl,
    id
)
