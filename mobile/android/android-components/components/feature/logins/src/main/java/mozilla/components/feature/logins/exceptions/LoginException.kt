/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.logins.exceptions

/**
 * A login exception.
 */
interface LoginException {
    /**
     * Unique ID of this login exception.
     */
    val id: Long

    /**
     * The origin of the login exception site.
     */
    val origin: String
}
