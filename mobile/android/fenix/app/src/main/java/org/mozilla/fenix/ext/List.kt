/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import org.mozilla.fenix.library.downloads.DownloadItem
import java.io.File

/**
 * Checks a List of DownloadItems to verify whether items
 * on that list are present on the disk or not. If a user has
 * deleted the downloaded item it should not show on the downloaded
 * list.
 */
fun List<DownloadItem>.filterNotExistsOnDisk(): List<DownloadItem> {
    return this.filter {
        File(it.filePath).exists()
    }
}
