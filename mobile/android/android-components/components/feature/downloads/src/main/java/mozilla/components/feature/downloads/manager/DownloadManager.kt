/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads.manager

import mozilla.components.browser.session.Download

typealias OnDownloadCompleted = (Download, Long) -> Unit

internal const val FILE_NOT_SUPPORTED = -1L

interface DownloadManager {

    var onDownloadCompleted: OnDownloadCompleted

    fun download(
        download: Download,
        refererUrl: String = "",
        cookie: String = ""
    ): Long

    fun unregisterListener()
}
