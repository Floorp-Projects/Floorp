/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.loginexceptions

import mozilla.components.feature.logins.exceptions.LoginException

/**
 * Interactor for the exceptions screen
 * Provides implementations for the ExceptionsViewInteractor
 */
class LoginExceptionsInteractor(
    private val deleteOne: (LoginException) -> Unit,
    private val deleteAll: () -> Unit
) : ExceptionsViewInteractor {
    override fun onDeleteAll() {
        deleteAll.invoke()
    }

    override fun onDeleteOne(item: LoginException) {
        deleteOne.invoke(item)
    }
}
