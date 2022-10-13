/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads.ext

import androidx.core.net.toUri
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.concept.fetch.Headers
import mozilla.components.concept.fetch.Headers.Names.CONTENT_DISPOSITION
import mozilla.components.concept.fetch.Headers.Names.CONTENT_LENGTH
import mozilla.components.concept.fetch.Headers.Names.CONTENT_TYPE
import mozilla.components.support.ktx.kotlin.sanitizeFileName
import mozilla.components.support.utils.DownloadUtils
import java.io.InputStream
import java.net.URLConnection

internal fun DownloadState.isScheme(protocols: Iterable<String>): Boolean {
    val scheme = url.trim().toUri().scheme ?: return false
    return protocols.contains(scheme)
}

/**
 * Returns a copy of the download with some fields filled in based on values from a response.
 *
 * @param headers Headers from the response.
 * @param stream Stream of the response body.
 */
internal fun DownloadState.withResponse(headers: Headers, stream: InputStream?): DownloadState {
    val contentDisposition = headers[CONTENT_DISPOSITION]
    var contentType = this.contentType
    if (contentType == null && stream != null) {
        contentType = URLConnection.guessContentTypeFromStream(stream)
    }
    if (contentType == null) {
        contentType = headers[CONTENT_TYPE]
    }

    val newFileName = if (fileName.isNullOrBlank()) {
        DownloadUtils.guessFileName(contentDisposition, destinationDirectory, url, contentType)
    } else {
        fileName
    }
    return copy(
        fileName = newFileName?.sanitizeFileName(),
        contentType = contentType,
        contentLength = contentLength ?: headers[CONTENT_LENGTH]?.toLongOrNull(),
    )
}
