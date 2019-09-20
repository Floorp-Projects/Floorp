/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.support.base.log.logger.Logger
import org.json.JSONException
import org.json.JSONObject
import java.io.File
import java.io.FileNotFoundException
import java.io.IOException
import java.lang.Exception

/**
 * Helper for importing/migrating "open tabs" from Fennec.
 */
object FennecSessionMigration {
    private val logger = Logger("FennecSessionImporter")

    /**
     * Ties to migrate the "open tabs" from the given [profilePath] and on success returns a
     * [SessionManager.Snapshot] (wrapped in [MigrationResult.Success]).
     */
    fun migrate(
        profilePath: File
    ): MigrationResult<SessionManager.Snapshot> {
        val sessionFiles = findSessionFiles(profilePath)
        if (sessionFiles.isEmpty()) {
            return MigrationResult.Failure(FileNotFoundException("No session store found"))
        }

        val failures = mutableListOf<Exception>()

        for (file in sessionFiles) {
            try {
                return parseJSON(file.readText())
            } catch (e: IOException) {
                logger.error("IOException while parsing Fennec session store", e)
                failures.add(e)
            } catch (e: JSONException) {
                logger.error("JSONException while parsing Fennec session store", e)
                failures.add(e)
            }
        }

        return MigrationResult.Failure(failures)
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

    @Throws(IOException::class, JSONException::class)
    private fun parseJSON(json: String): MigrationResult<SessionManager.Snapshot> {
        var selection = -1
        val sessions = mutableListOf<Session>()

        val windows = JSONObject(json).getJSONArray("windows")
        if (windows.length() == 0) {
            return MigrationResult.Success(SessionManager.Snapshot.empty())
        }

        val window = windows.getJSONObject(0)
        val tabs = window.getJSONArray("tabs")
        val selectedTab = window.optInt("selected", -1)

        for (i in 0 until tabs.length()) {
            val tab = tabs.getJSONObject(i)

            val index = tab.getInt("index")
            val entries = tab.getJSONArray("entries")
            if (index < 1 || entries.length() < index) {
                // This tab has no entries at all. Let's just stkip it.
                continue
            }

            val entry = entries.getJSONObject(index - 1)
            val url = entry.getString("url")
            val title = entry.optString("title").ifEmpty { url }
            val selected = selectedTab == i + 1

            sessions.add(Session(
                initialUrl = url
            ).also {
                it.title = title
            })

            if (selected) {
                selection = sessions.size - 1
            }
        }

        return MigrationResult.Success(SessionManager.Snapshot(
            sessions.map { SessionManager.Snapshot.Item(it) },
            selection
        ))
    }
}
