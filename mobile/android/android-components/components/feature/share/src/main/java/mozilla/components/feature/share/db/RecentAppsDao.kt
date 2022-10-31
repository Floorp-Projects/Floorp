/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.share.db

import androidx.room.Dao
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.Query
import androidx.room.Transaction

private const val DECAY_MULTIPLIER = 0.95

@Dao
internal abstract class RecentAppsDao {

    @Insert(onConflict = OnConflictStrategy.IGNORE)
    abstract fun insertRecentApps(recentApps: List<RecentAppEntity>)

    @Query(
        """
        DELETE FROM recent_apps_table
        WHERE activityName = :activityName
    """,
    )
    abstract fun deleteRecentApp(activityName: String)

    @Query(
        """
        SELECT * FROM recent_apps_table
        ORDER BY score DESC
        LIMIT :limit
    """,
    )
    abstract fun getRecentAppsUpTo(limit: Int): List<RecentAppEntity>

    /**
     * Increments the score of a recent app.
     * @param activityName - Name of the recent app to update.
     */
    @Query(
        """
        UPDATE recent_apps_table
        SET score = score + 1
        WHERE activityName = :activityName
    """,
    )
    abstract fun updateRecentAppScore(activityName: String)

    /**
     * Decreases the score of all but one app (exponential decay).
     * @param exceptActivity - ID of recent app to leave alone
     * @param decay - Amount to decay by. Should be between 0 and 1.
     */
    @Query(
        """
        UPDATE recent_apps_table
        SET score = score * :decay
        WHERE activityName != :exceptActivity
    """,
    )
    abstract fun decayAllRecentApps(
        exceptActivity: String,
        decay: Double = DECAY_MULTIPLIER,
    )

    @Transaction
    open fun updateRecentAppAndDecayRest(activityName: String) {
        updateRecentAppScore(activityName)
        decayAllRecentApps(exceptActivity = activityName)
    }
}
