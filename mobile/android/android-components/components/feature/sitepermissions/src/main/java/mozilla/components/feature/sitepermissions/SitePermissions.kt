/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sitepermissions

import android.os.Parcelable
import kotlinx.android.parcel.Parcelize
import mozilla.components.feature.sitepermissions.SitePermissions.Status.NO_DECISION
import mozilla.components.feature.sitepermissions.SitePermissionsStorage.Permission

/**
 * A site permissions and its state.
 */
@Parcelize
data class SitePermissions(
    val origin: String,
    val location: Status = NO_DECISION,
    val notification: Status = NO_DECISION,
    val microphone: Status = NO_DECISION,
    val camera: Status = NO_DECISION,
    val bluetooth: Status = NO_DECISION,
    val localStorage: Status = NO_DECISION,
    val savedAt: Long
) : Parcelable {
    enum class Status(
        internal val id: Int
    ) {
        BLOCKED(-1), NO_DECISION(0), ALLOWED(1);

        fun isAllowed() = this == ALLOWED

        fun doNotAskAgain() = this == ALLOWED || this == BLOCKED

        fun toggle(): Status = when (this) {
            BLOCKED, NO_DECISION -> ALLOWED
            ALLOWED -> BLOCKED
        }
    }

    /**
     * Gets the current status for a [Permission] type
     */
    operator fun get(permissionType: Permission): Status {
        return when (permissionType) {
            Permission.MICROPHONE -> microphone
            Permission.BLUETOOTH -> bluetooth
            Permission.CAMERA -> camera
            Permission.LOCAL_STORAGE -> localStorage
            Permission.NOTIFICATION -> notification
            Permission.LOCATION -> location
        }
    }
}
