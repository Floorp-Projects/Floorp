/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions.login

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.feature.logins.exceptions.LoginException
import mozilla.components.feature.logins.exceptions.LoginExceptionStorage
import org.mozilla.fenix.exceptions.ExceptionsInteractor

interface LoginExceptionsInteractor : ExceptionsInteractor<LoginException>

class DefaultLoginExceptionsInteractor(
    private val ioScope: CoroutineScope,
    private val loginExceptionStorage: LoginExceptionStorage,
) : LoginExceptionsInteractor {

    override fun onDeleteAll() {
        ioScope.launch {
            loginExceptionStorage.deleteAllLoginExceptions()
        }
    }

    override fun onDeleteOne(item: LoginException) {
        ioScope.launch {
            loginExceptionStorage.removeLoginException(item)
        }
    }
}
