/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads.ui

import android.annotation.SuppressLint
import android.content.pm.ResolveInfo
import android.os.Parcelable
import kotlinx.parcelize.Parcelize

/**
 * Represents an app that can perform downloads.
 *
 * @property name Name of the app.
 * @property resolver The [ResolveInfo] for this app.
 * @property packageName Package of the app.
 * @property activityName Activity that will be shared to.
 * @property url The full url to the content that should be downloaded.
 * @property contentType Content type (MIME type) to indicate the media type of the download.
 */
@SuppressLint("ParcelCreator")
@Parcelize
data class DownloaderApp(
    val name: String,
    val resolver: ResolveInfo,
    val packageName: String,
    val activityName: String,
    val url: String,
    val contentType: String?,
) : Parcelable
