/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import org.mozilla.fenix.R
import org.mozilla.fenix.library.downloads.DownloadItem

// While this looks complex, it's actually pretty simple.
@SuppressWarnings("ComplexMethod")
fun DownloadItem.getIcon(): Int {
    fun getIconCornerCases(fileName: String?): Int {
        return when {
            fileName?.endsWith("apk") == true -> R.drawable.ic_file_type_apk
            fileName?.endsWith("zip") == true -> R.drawable.ic_file_type_zip
            else -> R.drawable.ic_file_type_default
        }
    }

    fun checkForApplicationArchiveSubtypes(contentType: String): Int? {
        return when {
            contentType.contains("rar") -> R.drawable.ic_file_type_zip
            contentType.contains("zip") -> R.drawable.ic_file_type_zip
            contentType.contains("7z") -> R.drawable.ic_file_type_zip
            contentType.contains("tar") -> R.drawable.ic_file_type_zip
            contentType.contains("freearc") -> R.drawable.ic_file_type_zip
            contentType.contains("octet-stream") -> null
            contentType.contains("vnd.android.package-archive") -> null
            else -> R.drawable.ic_file_type_document
        }
    }

    fun getIconFromContentType(contentType: String): Int? {
        return when {
            contentType.contains("image/") -> R.drawable.ic_file_type_image
            contentType.contains("audio/") -> R.drawable.ic_file_type_audio_note
            contentType.contains("video/") -> R.drawable.ic_file_type_video
            contentType.contains("application/") -> checkForApplicationArchiveSubtypes(contentType)
            contentType.contains("text/") -> R.drawable.ic_file_type_document
            else -> null
        }
    }

    return contentType?.let { contentType ->
        getIconFromContentType(contentType)
    } ?: getIconCornerCases(fileName)
}
