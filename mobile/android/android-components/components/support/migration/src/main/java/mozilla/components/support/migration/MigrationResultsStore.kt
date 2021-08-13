/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import android.content.Context
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.utils.SharedPreferencesCache
import org.json.JSONArray
import org.json.JSONObject
import java.lang.IllegalStateException

/**
 * Results of running a single versioned migration.
 */
data class MigrationRun(val version: Int, val success: Boolean)

/**
 * Results of running a set of migrations.
 */
typealias MigrationResults = Map<Migration, MigrationRun>

/**
 * A storage layer for persisting [MigrationResults].
 */
internal class MigrationResultsStore(context: Context) : SharedPreferencesCache<MigrationResults>(context) {
    override val logger = Logger("MigrationResultsStore")
    override val cacheKey = "MigrationResultsStore"
    override val cacheName = cacheKey

    override fun MigrationResults.toJSON(): JSONObject {
        return JSONObject().also { result ->
            result.put(
                "list",
                JSONArray().also { list ->
                    this.forEach {
                        list.put(
                            JSONObject().also { run ->
                                run.put("migration", it.key.canonicalName)
                                run.put("version", it.value.version)
                                run.put("success", it.value.success)
                            }
                        )
                    }
                }
            )
        }
    }

    @Suppress("ComplexMethod")
    override fun fromJSON(obj: JSONObject): MigrationResults {
        val result = mutableMapOf<Migration, MigrationRun>()
        val list = obj.optJSONArray("list") ?: throw IllegalStateException("Corrupt migration history")

        for (i in 0 until list.length()) {
            val tuple = (list[i] as JSONObject)
            val migrationName = tuple.getString("migration")
            val migrationVersion = tuple.getInt("version")
            val migrationSuccess = tuple.getBoolean("success")

            val migration = when {
                // There are three possible states we can see on disk:
                // - Migration$History - what `Migration.History.javaClass.simpleName` produced before 92
                // - History - what `Migration.History.javaClass.simpleName` started to produce 92+
                // - some unknown theoretical state depending on unknown simpleName behaviour
                // -- assuming that this actually worked, and matched the class names
                // -- could be a full canonical name, maybe?
                // -- assuming that it contained 'history' in some form

                // So, since simpleName behaviour changed starting at 92, this code now covers all
                // of these possible cases (instead of just relying on simpleName for matching what's on disk).

                // Without this expanded check, we would see 'Migration$History' on disk and check
                // against 'History', failing and crashing.

                // The `Migration.History.canonicalName` matches the new behaviour of simpleName,
                // but it's hardcoded for future stability. Any builds that run migration code for the first
                // time will now persist hardcoded value from Migration.History.canonicalName instead of
                // simpleName output, which could potentially change.

                // In worst case scenario and out of abundance caution we also do a `contains` check.
                // There are three blocks, for performance reasons (first block covers most builds,
                // last block is slowest and hopefully is never hit).

                // These checks will work for most builds.
                migrationName == "Migration\$History" -> Migration.History
                migrationName == "Migration\$Bookmarks" -> Migration.Bookmarks
                migrationName == "Migration\$OpenTabs" -> Migration.OpenTabs
                migrationName == "Migration\$Gecko" -> Migration.Gecko
                migrationName == "Migration\$FxA" -> Migration.FxA
                migrationName == "Migration\$Logins" -> Migration.Logins
                migrationName == "Migration\$Settings" -> Migration.Settings
                migrationName == "Migration\$Addons" -> Migration.Addons
                migrationName == "Migration\$TelemetryIdentifiers" -> Migration.TelemetryIdentifiers
                migrationName == "Migration\$SearchEngine" -> Migration.SearchEngine
                migrationName == "Migration\$PinnedSites" -> Migration.PinnedSites

                // These checks will work for builds that migrated at 92+.
                migrationName == Migration.History.canonicalName -> Migration.History
                migrationName == Migration.Bookmarks.canonicalName -> Migration.Bookmarks
                migrationName == Migration.OpenTabs.canonicalName -> Migration.OpenTabs
                migrationName == Migration.Gecko.canonicalName -> Migration.Gecko
                migrationName == Migration.FxA.canonicalName -> Migration.FxA
                migrationName == Migration.Logins.canonicalName -> Migration.Logins
                migrationName == Migration.Settings.canonicalName -> Migration.Settings
                migrationName == Migration.Addons.canonicalName -> Migration.Addons
                migrationName == Migration.TelemetryIdentifiers.canonicalName -> Migration.TelemetryIdentifiers
                migrationName == Migration.SearchEngine.canonicalName -> Migration.SearchEngine
                migrationName == Migration.PinnedSites.canonicalName -> Migration.PinnedSites

                // Backup checks for hypothetical simpleName variants that didn't match above.
                migrationName.contains("History", ignoreCase = true) -> Migration.History
                migrationName.contains("Bookmarks", ignoreCase = true) -> Migration.Bookmarks
                migrationName.contains("OpenTabs", ignoreCase = true) -> Migration.OpenTabs
                migrationName.contains("Gecko", ignoreCase = true) -> Migration.Gecko
                migrationName.contains("FxA", ignoreCase = true) -> Migration.FxA
                migrationName.contains("Logins", ignoreCase = true) -> Migration.Logins
                migrationName.contains("Settings", ignoreCase = true) -> Migration.Settings
                migrationName.contains("Addons", ignoreCase = true) -> Migration.Addons
                migrationName.contains("TelemetryIdentifiers", ignoreCase = true) -> Migration.TelemetryIdentifiers
                migrationName.contains("SearchEngine", ignoreCase = true) -> Migration.SearchEngine
                migrationName.contains("PinnedSites", ignoreCase = true) -> Migration.PinnedSites

                else -> throw IllegalStateException(
                    "Unrecognized migration type: $migrationName; " +
                        "simpleName of History: ${Migration.History.javaClass.simpleName}"
                )
            }
            result[migration] = MigrationRun(version = migrationVersion, success = migrationSuccess)
        }
        return result
    }

    fun setOrUpdate(history: MigrationResults) {
        val cached = getCached()?.toMutableMap()

        if (cached == null) {
            setToCache(history)
            return
        }

        for (migration in history) {
            cached[migration.key] = migration.value
        }

        setToCache(cached)
    }
}
