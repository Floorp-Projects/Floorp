/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.firstrun

import android.support.annotation.VisibleForTesting
import mozilla.components.support.base.log.logger.Logger
import java.io.File
import java.io.IOException

/**
 * An implementation that uses the presence of a specific file in the
 * User's app directory to determine if this is a first run or not.
 *
 * @param gleanDataDir the directory, within the application's data dir, that
 *        contains Glean files.
 */
internal class FileFirstRunDetector(
    private val gleanDataDir: File
) {
    private val logger = Logger("glean/FileFirstRunDetector")

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    companion object {
        const val FIRST_RUN_FILENAME = "glean_already_ran"
    }

    private val firstRun: Boolean by lazy {
        !checkFirstRunFileExists()
    }

    /**
     * Whether or not the first run file exists in the Glean's data directory.
     *
     * @return true if the file exists (regardless if that's a dir or a file), false
     *         if it doesn't exist or an exception was thrown.
     */
    private fun checkFirstRunFileExists(): Boolean {
        val f = File(gleanDataDir, FIRST_RUN_FILENAME)
        return f.exists()
    }

    /**
     * Create the first run file
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    fun createFirstRunFile() {
        val f = File(gleanDataDir, FIRST_RUN_FILENAME)
        try {
            f.createNewFile()
        } catch (e: IOException) {
            logger.error("There was a problem creating the first run file ${f.absolutePath}")
        }
    }

    /**
     * Check if this is the first time Glean was run.
     *
     * @return true if it was, false otherwise
     */
    fun isFirstRun(): Boolean {
        if (firstRun) {
            createFirstRunFile()
            return true
        }

        return false
    }

    /**
     * Delete the first run file. This is useful for resetting
     * the first run state and run again any dependant initialization
     * code (e.g. in case of profile resets or opting out and then opting
     * back in).
     */
    fun reset() {
        val f = File(gleanDataDir, FIRST_RUN_FILENAME)
        f.delete()
    }
}
