/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.permission

import android.annotation.SuppressLint
import android.os.Parcelable
import kotlinx.parcelize.Parcelize
import mozilla.components.concept.engine.permission.SitePermissions.Status.NO_DECISION
import mozilla.components.concept.engine.permission.SitePermissionsStorage.Permission

/**
 * A site permissions and its state.
 */
@SuppressLint("ParcelCreator")
@Parcelize
data class SitePermissions(
    val origin: String,
    val location: Status = NO_DECISION,
    val notification: Status = NO_DECISION,
    val microphone: Status = NO_DECISION,
    val camera: Status = NO_DECISION,
    val bluetooth: Status = NO_DECISION,
    val localStorage: Status = NO_DECISION,
    val autoplayAudible: AutoplayStatus = AutoplayStatus.BLOCKED,
    val autoplayInaudible: AutoplayStatus = AutoplayStatus.ALLOWED,
    val mediaKeySystemAccess: Status = NO_DECISION,
    val crossOriginStorageAccess: Status = NO_DECISION,
    val savedAt: Long,
) : Parcelable {
    enum class Status(
        val id: Int,
    ) {
        BLOCKED(-1), NO_DECISION(0), ALLOWED(1);

        fun isAllowed() = this == ALLOWED

        fun doNotAskAgain() = this == ALLOWED || this == BLOCKED

        fun toggle(): Status = when (this) {
            BLOCKED, NO_DECISION -> ALLOWED
            ALLOWED -> BLOCKED
        }

        /**
         * Converts from [SitePermissions.Status] to [AutoplayStatus].
         */
        fun toAutoplayStatus(): AutoplayStatus {
            return when (this) {
                NO_DECISION, BLOCKED -> AutoplayStatus.BLOCKED
                ALLOWED -> AutoplayStatus.ALLOWED
            }
        }
    }

    /**
     * An enum that represents the status that autoplay can have.
     */
    enum class AutoplayStatus(val id: Int) {
        BLOCKED(Status.BLOCKED.id), ALLOWED(Status.ALLOWED.id);

        /**
         * Indicates if the status is allowed.
         */
        fun isAllowed() = this == ALLOWED

        /**
         * Convert from a AutoplayStatus to Status.
         */
        fun toStatus(): Status {
            return when (this) {
                BLOCKED -> Status.BLOCKED
                ALLOWED -> Status.ALLOWED
            }
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
            Permission.AUTOPLAY_AUDIBLE -> autoplayAudible.toStatus()
            Permission.AUTOPLAY_INAUDIBLE -> autoplayInaudible.toStatus()
            Permission.MEDIA_KEY_SYSTEM_ACCESS -> mediaKeySystemAccess
            Permission.STORAGE_ACCESS -> crossOriginStorageAccess
        }
    }
}
