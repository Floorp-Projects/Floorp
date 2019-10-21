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
            result.put("list", JSONArray().also { list ->
                this.forEach {
                    list.put(JSONObject().also { run ->
                        run.put("migration", it.key.javaClass.simpleName)
                        run.put("version", it.value.version)
                        run.put("success", it.value.success)
                    })
                }
            })
        }
    }

    override fun fromJSON(obj: JSONObject): MigrationResults {
        val result = mutableMapOf<Migration, MigrationRun>()
        val list = obj.optJSONArray("list") ?: throw IllegalStateException("Corrupt migration history")

        for (i in 0 until list.length()) {
            val tuple = (list[i] as JSONObject)
            val migrationName = tuple.getString("migration")
            val migrationVersion = tuple.getInt("version")
            val migrationSuccess = tuple.getBoolean("success")
            val migration = when (migrationName) {
                Migration.History.javaClass.simpleName -> Migration.History
                Migration.Bookmarks.javaClass.simpleName -> Migration.Bookmarks
                Migration.OpenTabs.javaClass.simpleName -> Migration.OpenTabs
                else -> throw IllegalStateException("Unrecognized migration type: $migrationName")
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
