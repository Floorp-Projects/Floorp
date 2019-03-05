/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sitepermissions

import mozilla.components.feature.sitepermissions.SitePermissions.Status.NO_DECISION

/**
 * A site permissions and its state.
 */
data class SitePermissions(
    val origin: String,
    val location: Status = NO_DECISION,
    val notification: Status = NO_DECISION,
    val microphone: Status = NO_DECISION,
    val cameraBack: Status = NO_DECISION,
    val cameraFront: Status = NO_DECISION,
    val bluetooth: Status = NO_DECISION,
    val localStorage: Status = NO_DECISION,
    val savedAt: Long
) {
    enum class Status(
        internal val id: Int
    ) {
        BLOCKED(-1), NO_DECISION(0), ALLOWED(1);

        fun isAllowed() = this == ALLOWED

        fun doNotAskAgain() = this == ALLOWED || this == BLOCKED
    }
}
