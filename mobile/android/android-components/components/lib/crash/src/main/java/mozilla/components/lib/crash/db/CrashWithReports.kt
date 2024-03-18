/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.db

import androidx.room.Embedded
import androidx.room.Relation

/**
 * Data class modelling the relationship between [CrashEntity] and [ReportEntity] objects.
 */
internal data class CrashWithReports(
    @Embedded
    val crash: CrashEntity,

    @Relation(
        parentColumn = "uuid",
        entityColumn = "crash_uuid",
    )
    val reports: List<ReportEntity>,
)
