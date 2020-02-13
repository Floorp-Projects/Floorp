/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.share

import android.content.Context
import androidx.annotation.VisibleForTesting
import mozilla.components.feature.share.adapter.RecentAppAdapter
import mozilla.components.feature.share.db.RecentAppEntity
import mozilla.components.feature.share.db.RecentAppsDatabase

/**
 * Class used for storing and retrieving the most recent apps
 */
class RecentAppsStorage(context: Context) {

    companion object {
        @VisibleForTesting
        internal const val DEFAULT_COUNT = 0.0
        private const val DECAY_MULTIPLIER = 0.95
    }

    internal var database: Lazy<RecentAppsDatabase> = lazy { RecentAppsDatabase.get(context) }

    /**
     * Increment the value stored in the database for the selected app. Then, apply a decay to all
     * other apps in the database. We do not need to handle overflow as it's not reasonably expected
     * to reach Double.MAX_VALUE for users
     */
    fun updateRecentApp(selectedPackageName: String) {
        val recentAppEntity = database.value.recentAppsDao().getRecentApp(selectedPackageName)
        recentAppEntity?.let {
            it.score += 1
            database.value.recentAppsDao().updateRecentApp(it)
            applyDecayToRecentAppsExceptFor(it.packageName)
        }
    }

    /**
     * Get a descending ordered list of the most recent apps
     * @param limit - size of list
     */
    fun getRecentAppsUpTo(limit: Int): List<RecentApp> {
        return database.value.recentAppsDao().getRecentAppsUpTo(limit).map { entity ->
            RecentAppAdapter(entity)
        }
    }

    /**
     * If there are apps that could resolve our share and are not added in our database, we add them
     * with a 0 count, so they can be updated later when a user uses that app
     */
    fun updateDatabaseWithNewApps(packageNames: List<String>) {
        for (packageName in packageNames) {
            if (database.value.recentAppsDao().getRecentApp(packageName) == null) {
                val recentAppEntity = RecentAppEntity(
                    packageName = packageName,
                    score = DEFAULT_COUNT
                )
                database.value.recentAppsDao().insertRecentApp(recentAppEntity)
            }
        }
    }

    /**
     * Decreases the score of the apps as they become older (exponential decay). This allows newly
     * installed apps to catch up and appear in the most recent section faster.
     */
    private fun applyDecayToRecentAppsExceptFor(selectedPackageName: String) {
        val recentApps = database.value.recentAppsDao().getAllRecentApps()
        for (app in recentApps) {
            if (app.packageName != selectedPackageName && app.score != DEFAULT_COUNT) {
                app.score *= DECAY_MULTIPLIER
            }
        }
        database.value.recentAppsDao().updateAllRecentApp(recentApps)
    }
}
