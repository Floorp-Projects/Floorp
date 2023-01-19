/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.os.Bundle
import androidx.appcompat.app.AppCompatDialogFragment
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.feature.downloads.DownloadDialogFragment.Companion.BYTES_TO_MB_LIMIT
import mozilla.components.feature.downloads.DownloadDialogFragment.Companion.KILOBYTE
import mozilla.components.feature.downloads.DownloadDialogFragment.Companion.MEGABYTE
import mozilla.components.support.utils.DownloadUtils

/**
 * This is a general representation of a dialog meant to be used in collaboration with [DownloadsFeature]
 * to show a dialog before a download is triggered.
 * If [SimpleDownloadDialogFragment] is not flexible enough for your use case you should inherit for this class.
 * Be mindful to call [onStartDownload] when you want to start the download.
 */
abstract class DownloadDialogFragment : AppCompatDialogFragment() {

    /**
     * A callback to trigger a download, call it when you are ready to start a download. For instance,
     * a valid use case can be in confirmation dialog, after the positive button is clicked,
     * this callback must be called.
     */
    var onStartDownload: () -> Unit = {}

    var onCancelDownload: () -> Unit = {}

    /**
     * Add the metadata of this download object to the arguments of this fragment.
     */
    fun setDownload(download: DownloadState) {
        val args = arguments ?: Bundle()
        args.putString(
            KEY_FILE_NAME,
            download.fileName
                ?: DownloadUtils.guessFileName(null, download.destinationDirectory, download.url, download.contentType),
        )
        args.putString(KEY_URL, download.url)
        args.putLong(KEY_CONTENT_LENGTH, download.contentLength ?: 0)
        arguments = args
    }

    companion object {
        /**
         * Key for finding the file name in the arguments.
         */
        const val KEY_FILE_NAME = "KEY_FILE_NAME"

        /**
         * Key for finding the content length in the arguments.
         */
        const val KEY_CONTENT_LENGTH = "KEY_CONTENT_LENGTH"

        /**
         * Key for finding the url in the arguments.
         */
        const val KEY_URL = "KEY_URL"

        const val FRAGMENT_TAG = "SHOULD_DOWNLOAD_PROMPT_DIALOG"

        const val MEGABYTE = 1024.0 * 1024.0

        const val KILOBYTE = 1024.0

        const val BYTES_TO_MB_LIMIT = 0.01
    }
}

/**
 * Converts the bytes to megabytes with two decimal places and returns a formatted string
 */
fun Long.toMegabyteString(): String {
    return String.format("%.2f MB", this / MEGABYTE)
}

/**
 * Converts the bytes to kilobytes with two decimal places and returns a formatted string
 */
fun Long.toKilobyteString(): String {
    return String.format("%.2f KB", this / KILOBYTE)
}

/**
 * Converts the bytes to megabytes or kilobytes( if size smaller than 0.01 MB)
 * with two decimal places and returns a formatted string
 */
fun Long.toMegabyteOrKilobyteString(): String {
    return if (this / MEGABYTE < BYTES_TO_MB_LIMIT) {
        this.toKilobyteString()
    } else {
        this.toMegabyteString()
    }
}
