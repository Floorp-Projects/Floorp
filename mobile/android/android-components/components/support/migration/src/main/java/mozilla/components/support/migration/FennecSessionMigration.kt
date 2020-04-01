/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import android.net.Uri
import mozilla.components.browser.session.SessionManager
import mozilla.components.support.base.crash.CrashReporting
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
        crashReporter: CrashReporting
    ): Result<SessionManager.Snapshot> {
        val sessionFiles = findSessionFiles(profilePath)
        if (sessionFiles.isEmpty()) {
            return Result.Failure(FileNotFoundException("No session store found"))
        }

        val failures = mutableListOf<Exception>()

        for (file in sessionFiles) {
            try {
                return StreamingSessionStoreParser.parse(file).filter(logger, crashReporter)
            } catch (e: FileNotFoundException) {
                logger.warn("FileNotFoundException while trying to parse session store", e)
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

@Suppress("TooGenericExceptionCaught")
private fun Result.Success<SessionManager.Snapshot>.filter(
    logger: Logger,
    crashReporter: CrashReporting
): Result.Success<SessionManager.Snapshot> {
    var selectedIndex = value.selectedSessionIndex

    logger.debug("Filtering migrated sessions (size=${value.sessions.size}, index=$selectedIndex)")

    val sessions = value.sessions.mapIndexedNotNull { index, item ->
        when {
            // We filter out about:home tabs since Fenix does not handle those URLs.
            item.session.url == ABOUT_HOME -> {
                logger.debug("- Filtering about:home URL")
                if (index < selectedIndex) {
                    selectedIndex--
                }
                null
            }

            // We rewrite about:reader URLs to their actual URLs. Fenix does not handle about:reader
            // URLs, but it can load the original URL.
            item.session.url.startsWith(ABOUT_READER) -> {
                try {
                    val url = Uri.decode(item.session.url.substring(ABOUT_READER.length))

                    logger.debug("- Rewriting about:reader URL (${item.session.url}): $url")

                    item.session.url = url
                    item
                } catch (e: Exception) {
                    crashReporter.submitCaughtException(e)
                    null
                }
            }

            else -> item
        }
    }

    if (sessions.isEmpty()) {
        selectedIndex = -1
    }

    logger.debug("Migrated sessions filtered (size=${sessions.size}, index=$selectedIndex)")

    return Result.Success(SessionManager.Snapshot(sessions, selectedIndex))
}

private const val ABOUT_HOME = "about:home"
private const val ABOUT_READER = "about:reader?url="
