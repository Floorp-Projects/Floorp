/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.db

import androidx.lifecycle.LiveData
import androidx.room.Dao
import androidx.room.Insert
import androidx.room.Query
import androidx.room.Transaction

/**
 * Dao for saving and accessing crash related information.
 */
@Dao
internal interface CrashDao {
    /**
     * Inserts a crash into the database.
     */
    @Insert
    fun insertCrash(crash: CrashEntity): Long

    /**
     * Inserts a report to the database.
     */
    @Insert
    fun insertReport(report: ReportEntity): Long

    /**
     * Returns saved crashes with their reports.
     */
    @Transaction
    @Query("SELECT * FROM crashes ORDER BY created_at DESC")
    fun getCrashesWithReports(): LiveData<List<CrashWithReports>>
}
