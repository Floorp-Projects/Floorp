/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.sync.logins

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.async
import mozilla.appservices.logins.InvalidLoginReason
import mozilla.appservices.logins.InvalidRecordException
import mozilla.components.concept.storage.Login
import mozilla.components.concept.storage.LoginValidationDelegate
import mozilla.components.concept.storage.LoginValidationDelegate.Result
import mozilla.components.lib.dataprotect.SecureAbove22Preferences

/**
 * A delegate that will check against [storage] to see if a given Login can be persisted, and return
 * information about why it can or cannot.
 */
class DefaultLoginValidationDelegate(
    private val storage: AsyncLoginsStorage,
    keyStore: SecureAbove22Preferences,
    private val scope: CoroutineScope = CoroutineScope(IO)
) : LoginValidationDelegate {

    private val password = { scope.async { keyStore.getString(PASSWORDS_KEY)!! } }

    @Suppress("ComplexMethod") // This method is not actually complex
    override fun validateCanPersist(login: Login): Deferred<Result> {
        return scope.async {
            try {
                storage.ensureUnlocked(password().await()).await()
                storage.ensureValid(login.toServerPassword()).await()
                Result.CanBeCreated
            } catch (e: InvalidRecordException) {
                when (e.reason) {
                    InvalidLoginReason.DUPLICATE_LOGIN -> Result.CanBeUpdated
                    InvalidLoginReason.EMPTY_PASSWORD -> Result.Error.EmptyPassword
                    InvalidLoginReason.EMPTY_ORIGIN -> Result.Error.GeckoError(e)
                    InvalidLoginReason.BOTH_TARGETS -> Result.Error.GeckoError(e)
                    InvalidLoginReason.NO_TARGET -> Result.Error.GeckoError(e)
                    // TODO in what ways can the login fields be illegal? represent these in the UI
                    InvalidLoginReason.ILLEGAL_FIELD_VALUE -> Result.Error.GeckoError(e)
                }
            } finally {
                @Suppress("DeferredResultUnused") // No action needed
                storage.lock()
            }
        }
    }
}
