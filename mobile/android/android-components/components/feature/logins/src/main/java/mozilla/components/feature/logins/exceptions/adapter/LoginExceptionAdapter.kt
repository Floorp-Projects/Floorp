/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.logins.exceptions.adapter

import mozilla.components.feature.logins.exceptions.LoginException
import mozilla.components.feature.logins.exceptions.db.LoginExceptionEntity

internal class LoginExceptionAdapter(
    internal val entity: LoginExceptionEntity,
) : LoginException {
    override val id: Long
        get() = entity.id!!

    override val origin: String
        get() = entity.origin

    override fun equals(other: Any?): Boolean {
        if (other !is LoginExceptionAdapter) {
            return false
        }

        return entity == other.entity
    }

    override fun hashCode(): Int {
        return entity.hashCode()
    }
}
