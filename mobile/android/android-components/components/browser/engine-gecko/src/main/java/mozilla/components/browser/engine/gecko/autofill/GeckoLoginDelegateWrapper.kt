/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.autofill

import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import mozilla.components.concept.storage.Login
import mozilla.components.concept.storage.LoginStorageDelegate
import org.mozilla.geckoview.Autocomplete
import org.mozilla.geckoview.GeckoResult

/**
 * This class exists only to convert incoming [LoginEntry] arguments into [Login]s, then forward
 * them to [storageDelegate]. This allows us to avoid duplicating [LoginStorageDelegate] code
 * between different versions of GeckoView, by duplicating this wrapper instead.
 */
class GeckoLoginDelegateWrapper(private val storageDelegate: LoginStorageDelegate) :
    Autocomplete.LoginStorageDelegate {

    override fun onLoginSave(login: Autocomplete.LoginEntry) {
        storageDelegate.onLoginSave(login.toLogin())
    }

    override fun onLoginFetch(domain: String): GeckoResult<Array<Autocomplete.LoginEntry>>? {
        val result = GeckoResult<Array<Autocomplete.LoginEntry>>()

        GlobalScope.launch(IO) {
            val storedLogins = storageDelegate.onLoginFetch(domain)

            val logins = storedLogins.await()
                .map { it.toLoginEntry() }
                .toTypedArray()

            result.complete(logins)
        }

        return result
    }
}

/**
 * Converts a GeckoView [LoginStorage.LoginEntry] to an Android Components [Login]
 */
private fun Autocomplete.LoginEntry.toLogin() = Login(
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
private fun Login.toLoginEntry() = Autocomplete.LoginEntry.Builder()
    .guid(guid)
    .origin(origin)
    .formActionOrigin(formActionOrigin)
    .httpRealm(httpRealm)
    .username(username)
    .password(password)
    .build()
