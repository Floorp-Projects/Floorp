/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.os.Bundle
import androidx.fragment.app.DialogFragment
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.support.utils.DownloadUtils

/**
 * This is a general representation of a dialog meant to be used in collaboration with [DownloadsFeature]
 * to show a dialog before a download is triggered.
 * If [SimpleDownloadDialogFragment] is not flexible enough for your use case you should inherit for this class.
 * Be mindful to call [onStartDownload] when you want to start the download.
 */
abstract class DownloadDialogFragment : DialogFragment() {

    /**
     * A callback to trigger a download, call it when you are ready to start a download. For instance,
     * a valid use case can be in confirmation dialog, after the positive button is clicked,
     * this callback must be called.
     */
    var onStartDownload: () -> Unit = {}

    var onCancelDownload: () -> Unit = {}

    /**
     * add the metadata of this download object to the arguments of this fragment.
     */
    fun setDownload(download: DownloadState) {
        val args = arguments ?: Bundle()
        args.putString(KEY_FILE_NAME, download.fileName
            ?: DownloadUtils.guessFileName(null, download.url, download.contentType))
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
    }
}
