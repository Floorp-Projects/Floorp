/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.update.db

import androidx.room.ColumnInfo
import androidx.room.Entity
import androidx.room.PrimaryKey
import mozilla.components.feature.addons.update.AddonUpdater
import mozilla.components.feature.addons.update.AddonUpdater.Status.Error
import mozilla.components.feature.addons.update.AddonUpdater.Status.NoUpdateAvailable
import mozilla.components.feature.addons.update.AddonUpdater.Status.NotInstalled
import mozilla.components.feature.addons.update.AddonUpdater.Status.SuccessfullyUpdated
import mozilla.components.feature.addons.update.db.UpdateAttemptEntity.Companion.ERROR_DB
import mozilla.components.feature.addons.update.db.UpdateAttemptEntity.Companion.NOT_INSTALLED_DB
import mozilla.components.feature.addons.update.db.UpdateAttemptEntity.Companion.NO_UPDATE_AVAILABLE_DB
import mozilla.components.feature.addons.update.db.UpdateAttemptEntity.Companion.SUCCESSFULLY_UPDATED_DB
import java.util.Date

/**
 * Internal entity representing a [AddonUpdater.UpdateAttempt] as it gets saved to the database.
 */
@Entity(tableName = "update_attempts")
internal data class UpdateAttemptEntity(
    @PrimaryKey
    @ColumnInfo(name = "addon_id")
    var addonId: String,

    @ColumnInfo(name = "date")
    var date: Long,

    @ColumnInfo(name = "status")
    var status: Int,

    @ColumnInfo(name = "error_message")
    var errorMessage: String = "",

    @ColumnInfo(name = "error_trace")
    var errorTrace: String = "",
) {
    internal fun toUpdateAttempt(): AddonUpdater.UpdateAttempt {
        return AddonUpdater.UpdateAttempt(addonId, Date(date), toStatus())
    }

    companion object {
        const val NOT_INSTALLED_DB = 0
        const val SUCCESSFULLY_UPDATED_DB = 1
        const val NO_UPDATE_AVAILABLE_DB = 2
        const val ERROR_DB = 3
    }

    internal fun toStatus(): AddonUpdater.Status? {
        return when (status) {
            NOT_INSTALLED_DB -> NotInstalled
            SUCCESSFULLY_UPDATED_DB -> SuccessfullyUpdated
            NO_UPDATE_AVAILABLE_DB -> NoUpdateAvailable
            ERROR_DB -> Error(errorMessage, Exception(errorTrace))
            else -> null
        }
    }
}

internal fun AddonUpdater.Status.toDB(): Int {
    return when (this) {
        NotInstalled -> NOT_INSTALLED_DB
        SuccessfullyUpdated -> SUCCESSFULLY_UPDATED_DB
        NoUpdateAvailable -> NO_UPDATE_AVAILABLE_DB
        is Error -> ERROR_DB
    }
}

internal fun AddonUpdater.UpdateAttempt.toEntity(): UpdateAttemptEntity {
    var message = ""
    var errorTrace = ""
    if (status is Error) {
        message = this.status.message
        errorTrace = this.status.exception.stackTrace.first().toString()
    }
    return UpdateAttemptEntity(addonId, date.time, status?.toDB() ?: -1, message, errorTrace)
}
