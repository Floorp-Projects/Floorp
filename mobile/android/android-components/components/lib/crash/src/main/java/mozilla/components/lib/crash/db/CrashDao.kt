/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.db

import android.annotation.SuppressLint
import android.util.Log
import androidx.lifecycle.LiveData
import androidx.room.Dao
import androidx.room.Insert
import androidx.room.Query
import androidx.room.Transaction
import java.lang.Exception

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

    /**
     * Delete table.
     */
    @Transaction
    @Query("DELETE FROM crashes")
    fun deleteAll()
}

/**
 * Insert crash into database safely, ignoring any exceptions.
 *
 * When handling a crash we want to avoid causing another crash when writing to the database. In the
 * case of an error we will just ignore it and continue without saving to the database.
 */
@SuppressLint("LogUsage") // We do not want to use our custom logger while handling the crash
@Suppress("TooGenericExceptionCaught")
internal fun CrashDao.insertCrashSafely(entity: CrashEntity) {
    try {
        insertCrash(entity)
    } catch (e: Exception) {
        Log.e("CrashDao", "Failed to insert crash into database", e)
    }
}

/**
 * Insert report into database safely, ignoring any exceptions.
 *
 * When handling a crash we want to avoid causing another crash when writing to the database. In the
 * case of an error we will just ignore it and continue without saving to the database.
 */
@SuppressLint("LogUsage") // We do not want to use our custom logger while handling the crash
@Suppress("TooGenericExceptionCaught")
internal fun CrashDao.insertReportSafely(entity: ReportEntity) {
    try {
        insertReport(entity)
    } catch (e: Exception) {
        Log.e("CrashDao", "Failed to insert report into database", e)
    }
}
