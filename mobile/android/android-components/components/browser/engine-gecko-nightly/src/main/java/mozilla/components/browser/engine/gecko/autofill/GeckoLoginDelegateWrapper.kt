/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.autofill

import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import mozilla.components.concept.storage.Login
import mozilla.components.concept.storage.LoginStorageDelegate
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.LoginStorage

/**
 * This class exists only to convert incoming [LoginEntry] arguments into [Login]s, then forward
 * them to [storageDelegate]. This allows us to avoid duplicating [LoginStorageDelegate] code
 * between different versions of GeckoView, by duplicating this wrapper instead.
 */
class GeckoLoginDelegateWrapper(private val storageDelegate: LoginStorageDelegate) :
    LoginStorage.Delegate {

    override fun onLoginSave(login: LoginStorage.LoginEntry) {
        storageDelegate.onLoginSave(login.toLogin())
    }

    override fun onLoginFetch(domain: String): GeckoResult<Array<LoginStorage.LoginEntry>>? {
        val result = GeckoResult<Array<LoginStorage.LoginEntry>>()

        GlobalScope.launch(IO) {
            val storedLogins = storageDelegate.onLoginFetch(domain)

            val logins = storedLogins.await()
                .map { it.toLoginEntry() }
                .toTypedArray()

            result.complete(logins)
        }

        return result
    }

    /**
     * This method has not yet been implemented in GV. Once it has, we should add an override to it
     * here.
     */
    fun onLoginUsed(login: LoginStorage.LoginEntry) {
        storageDelegate.onLoginUsed(login.toLogin())
    }
}

/**
 * Converts a GeckoView [LoginStorage.LoginEntry] to an Android Components [Login]
 */
private fun LoginStorage.LoginEntry.toLogin() = Login(
    guid = guid,
    origin = origin,
    formActionOrigin = formActionOrigin,
    httpRealm = httpRealm,
    username = username,
    password = password
)

/**
 * Converts an Android Components [Login] to a GeckoView [LoginStorage.LoginEntry]
 */
private fun Login.toLoginEntry() = LoginStorage.LoginEntry.Builder()
    .guid(guid)
    .origin(origin)
    .formActionOrigin(formActionOrigin)
    .httpRealm(httpRealm)
    .username(username)
    .password(password)
    .build()
