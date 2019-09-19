/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads.ext

import android.content.Intent
import androidx.core.os.bundleOf
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.concept.fetch.Headers.Names.CONTENT_LENGTH
import mozilla.components.concept.fetch.Headers.Names.CONTENT_TYPE
import mozilla.components.concept.fetch.Headers.Names.REFERRER
import mozilla.components.concept.fetch.Headers.Names.USER_AGENT

private const val INTENT_DOWNLOAD = "mozilla.components.feature.downloads.DOWNLOAD"
private const val INTENT_URL = "mozilla.components.feature.downloads.URL"
private const val INTENT_FILE_NAME = "mozilla.components.feature.downloads.FILE_NAME"
private const val INTENT_DESTINATION = "mozilla.components.feature.downloads.DESTINATION"

fun Intent.putDownloadExtra(download: DownloadState) {
    download.apply {
        putExtra(INTENT_DOWNLOAD, bundleOf(
            INTENT_URL to url,
            INTENT_FILE_NAME to fileName,
            CONTENT_TYPE to contentType,
            CONTENT_LENGTH to contentLength,
            USER_AGENT to userAgent,
            INTENT_DESTINATION to destinationDirectory,
            REFERRER to referrerUrl
        ))
    }
}

fun Intent.getDownloadExtra(): DownloadState? =
    getBundleExtra(INTENT_DOWNLOAD)?.run {
        val url = getString(INTENT_URL)
        val fileName = getString(INTENT_FILE_NAME)
        val destination = getString(INTENT_DESTINATION)
        if (url == null || destination == null) return null

        DownloadState(
            url = url,
            fileName = fileName,
            contentType = getString(CONTENT_TYPE),
            contentLength = get(CONTENT_TYPE) as? Long?,
            userAgent = getString(USER_AGENT),
            destinationDirectory = destination,
            referrerUrl = getString(REFERRER)
        )
    }
