/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.sync.logins

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import mozilla.components.concept.storage.Login
import mozilla.components.concept.storage.LoginEntry
import mozilla.components.concept.storage.LoginStorageDelegate
import mozilla.components.concept.storage.LoginsStorage

/**
 * [LoginStorageDelegate] implementation.
 *
 * An abstraction that handles the persistence and retrieval of [LoginEntry]s so that Gecko doesn't
 * have to.
 *
 * In order to use this class, attach it to the active [GeckoRuntime] as its `loginStorageDelegate`.
 * It is not designed to work with other engines.
 *
 * This class is part of a complex flow integrating Gecko and Application Services code, which is
 * described here:
 *
 * - GV finds something on a page that it believes could be autofilled
 * - GV calls [onLoginFetch]
 *   - We retrieve all [Login]s with matching domains (if any) from [loginStorage]
 *   - We return these [Login]s to GV
 * - GV autofills one of the returned [Login]s into the page
 *   - GV calls [onLoginUsed] to let us know which [Login] was used
 * - User submits their credentials
 * - GV detects something that looks like a login submission
 *   - ([GeckoLoginStorageDelegate] is not involved with this step)
 *   `SaveLoginDialogFragment` is shown to the user, who decides whether or not
 *   to save the [LoginEntry] and gives them a chance to manually adjust the
 *   username/password fields.
 *     - `SaveLoginDialogFragment` uses `DefaultLoginValidationDelegate` to determine
 *     what the result of the operation will be: saving a new login,
 *     updating an existing login, or filling in a blank username.
 *   - If the user accepts: GV calls [onLoginSave] with the [LoginEntry]
 */
class GeckoLoginStorageDelegate(
    private val loginStorage: Lazy<LoginsStorage>,
    private val scope: CoroutineScope = CoroutineScope(Dispatchers.IO),
) : LoginStorageDelegate {

    override fun onLoginUsed(login: Login) {
        scope.launch {
            loginStorage.value.touch(login.guid)
        }
    }

    override fun onLoginFetch(domain: String): Deferred<List<Login>> {
        return scope.async {
            loginStorage.value.getByBaseDomain(domain)
        }
    }

    @Synchronized
    override fun onLoginSave(login: LoginEntry) {
        scope.launch {
            loginStorage.value.addOrUpdate(login)
        }
    }
}
