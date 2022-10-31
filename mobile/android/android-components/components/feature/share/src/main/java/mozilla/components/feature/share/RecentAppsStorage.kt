/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.share

import android.content.Context
import androidx.annotation.VisibleForTesting
import mozilla.components.feature.share.db.RecentAppEntity
import mozilla.components.feature.share.db.RecentAppsDao
import mozilla.components.feature.share.db.RecentAppsDatabase

/**
 * Class used for storing and retrieving the most recent apps
 */
class RecentAppsStorage(context: Context) {

    @VisibleForTesting
    internal var recentAppsDao: Lazy<RecentAppsDao> = lazy {
        RecentAppsDatabase.get(context).recentAppsDao()
    }

    /**
     * Increment the value stored in the database for the selected app. Then, apply a decay to all
     * other apps in the database. This allows newly installed apps to catch up and appear in the
     * most recent section faster. We do not need to handle overflow as it's not reasonably expected
     * to reach Double.MAX_VALUE for users
     */
    fun updateRecentApp(selectedActivityName: String) {
        recentAppsDao.value.updateRecentAppAndDecayRest(selectedActivityName)
    }

    /**
     * Deletes an app form the recent apps list
     * @param activityName - name of the activity of the app
     */
    fun deleteRecentApp(activityName: String) {
        recentAppsDao.value.deleteRecentApp(activityName)
    }

    /**
     * Get a descending ordered list of the most recent apps
     * @param limit - size of list
     */
    fun getRecentAppsUpTo(limit: Int): List<RecentApp> {
        return recentAppsDao.value.getRecentAppsUpTo(limit)
    }

    /**
     * If there are apps that could resolve our share and are not added in our database, we add them
     * with a 0 count, so they can be updated later when a user uses that app
     */
    fun updateDatabaseWithNewApps(activityNames: List<String>) {
        recentAppsDao.value.insertRecentApps(
            activityNames.map { activityName ->
                RecentAppEntity(activityName)
            },
        )
    }
}
