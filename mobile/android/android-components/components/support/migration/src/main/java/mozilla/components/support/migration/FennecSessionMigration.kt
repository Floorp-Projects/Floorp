/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import mozilla.components.browser.session.SessionManager
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.migration.session.StreamingSessionStoreParser
import org.json.JSONException
import java.io.File
import java.io.FileNotFoundException
import java.io.IOException

/**
 * Helper for importing/migrating "open tabs" from Fennec.
 */
internal object FennecSessionMigration {
    private val logger = Logger("FennecSessionImporter")

    /**
     * Ties to migrate the "open tabs" from the given [profilePath] and on success returns a
     * [SessionManager.Snapshot] (wrapped in [Result.Success]).
     */
    fun migrate(
        profilePath: File,
        crashReporter: CrashReporter
    ): Result<SessionManager.Snapshot> {
        val sessionFiles = findSessionFiles(profilePath)
        if (sessionFiles.isEmpty()) {
            return Result.Failure(FileNotFoundException("No session store found"))
        }

        val failures = mutableListOf<Exception>()

        for (file in sessionFiles) {
            try {
                return StreamingSessionStoreParser.parse(file)
            } catch (e: FileNotFoundException) {
                logger.warn("FileNotFoundException while trying to parse sessopm stpre", e)
            } catch (e: IOException) {
                crashReporter.submitCaughtException(e)
                logger.error("IOException while parsing Fennec session store", e)
                failures.add(e)
            } catch (e: JSONException) {
                crashReporter.submitCaughtException(e)
                logger.error("JSONException while parsing Fennec session store", e)
                failures.add(e)
            } catch (e: IllegalStateException) {
                crashReporter.submitCaughtException(e)
                logger.error("IllegalStateException while parsing Fennec session store", e)
                failures.add(e)
            }
        }

        return Result.Failure(failures)
    }

    private fun findSessionFiles(profilePath: File): List<File> {
        val files = mutableListOf<File>()

        val defaultStore = File(profilePath, "sessionstore.js")
        if (defaultStore.exists()) {
            files.add(defaultStore)
        }

        val backupStore = File(profilePath, "sessionstore.bak")
        if (backupStore.exists()) {
            files.add(backupStore)
        }

        return files
    }
}
