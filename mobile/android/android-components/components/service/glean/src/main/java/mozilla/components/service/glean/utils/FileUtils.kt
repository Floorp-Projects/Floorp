/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.utils

import mozilla.components.support.base.log.logger.Logger
import java.io.File

private val logger: Logger = Logger("glean/FileUtils")

/**
 * Helper function to determine if the directory exists and attempts to create it if
 * it doesn't
 *
 * @param directory File representing the directory path
 */
internal fun ensureDirectoryExists(directory: File) {
    if (!directory.exists() && !directory.mkdirs()) {
        logger.error("Directory doesn't exist and can't be created: " + directory.absolutePath)
    }

    if (!directory.isDirectory || !directory.canWrite()) {
        logger.error("Directory is not writable directory: " + directory.absolutePath)
    }
}
