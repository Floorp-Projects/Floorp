/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.db

import androidx.room.ColumnInfo
import androidx.room.Entity
import androidx.room.PrimaryKey
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.service.CrashReporterService

/**
 * Datanase entry describing a crash report that was sent to a crash reporting service.
 */
@Entity(
    tableName = "reports",
)
internal data class ReportEntity(
    /**
     * Database internal primary key of the entry.
     */
    @PrimaryKey(autoGenerate = true)
    @ColumnInfo(name = "id")
    var id: Long? = null,

    /**
     * UUID of the crash that was reported.
     */
    @ColumnInfo(name = "crash_uuid")
    var crashUuid: String,

    /**
     * Id of the service the crash was reported to (matching [CrashReporterService.id].
     */
    @ColumnInfo(name = "service_id")
    var serviceId: String,

    /**
     * The id of the crash report as returned by [CrashReporterService.report].
     */
    @ColumnInfo(name = "report_id")
    var reportId: String,
)

internal fun CrashReporterService.toReportEntity(crash: Crash, reportId: String): ReportEntity {
    return ReportEntity(
        crashUuid = crash.uuid,
        serviceId = id,
        reportId = reportId,
    )
}
