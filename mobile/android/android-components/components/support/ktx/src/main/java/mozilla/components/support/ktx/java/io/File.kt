/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.java.io

import java.io.File

/**
 * Removes all files in the directory named by this abstract pathname. Does nothing if the [File] is not pointing to
 * a directory.
 */
fun File.truncateDirectory() {
    if (!isDirectory) {
        return
    }

    listFiles()?.forEach { file ->
        if (file.isDirectory) {
            file.truncateDirectory()
            file.delete()
        } else {
            file.delete()
        }
    }
}
