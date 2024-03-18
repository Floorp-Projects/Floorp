/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads.ext

import android.annotation.TargetApi
import android.app.DownloadManager
import android.content.Context
import android.net.Uri
import android.os.Build
import androidx.core.content.getSystemService

/**
 * Wraps around [DownloadManager.addCompletedDownload] and calls the correct
 * method depending on the SDK version.
 *
 * Deprecated in Android Q, use MediaStore on that version.
 */
@TargetApi(Build.VERSION_CODES.P)
@Suppress("Deprecation", "LongParameterList")
internal fun Context.addCompletedDownload(
    title: String,
    description: String,
    isMediaScannerScannable: Boolean,
    mimeType: String,
    path: String,
    length: Long,
    showNotification: Boolean,
    uri: Uri?,
    referer: Uri?,
) = getSystemService<DownloadManager>()!!.run {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
        addCompletedDownload(
            title,
            description,
            isMediaScannerScannable,
            mimeType,
            path,
            length,
            showNotification,
            uri,
            referer,
        )
    } else {
        addCompletedDownload(
            title,
            description,
            isMediaScannerScannable,
            mimeType,
            path,
            length,
            showNotification,
        )
    }
}
