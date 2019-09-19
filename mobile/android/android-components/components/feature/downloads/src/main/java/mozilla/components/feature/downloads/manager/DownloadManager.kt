/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads.manager

import android.content.Context
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.support.ktx.android.content.isPermissionGranted

typealias OnDownloadCompleted = (DownloadState, Long) -> Unit

interface DownloadManager {

    val permissions: Array<String>

    var onDownloadCompleted: OnDownloadCompleted

    /**
     * Schedules a download through the [DownloadManager].
     * @param download metadata related to the download.
     * @param cookie any additional cookie to add as part of the download request.
     * @return the id reference of the scheduled download.
     */
    fun download(
        download: DownloadState,
        cookie: String = ""
    ): Long?

    fun unregisterListeners() = Unit
}

fun DownloadManager.validatePermissionGranted(context: Context) {
    if (!context.isPermissionGranted(permissions.asIterable())) {
        throw SecurityException("You must be granted ${permissions.joinToString()}")
    }
}

internal val noop: OnDownloadCompleted = { _, _ -> }
