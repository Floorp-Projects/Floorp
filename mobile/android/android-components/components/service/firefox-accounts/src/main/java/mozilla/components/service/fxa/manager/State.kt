/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa.manager

import mozilla.components.concept.sync.AuthException
import mozilla.components.service.fxa.FxaAuthData
import mozilla.components.service.fxa.sharing.ShareableAccount

/**
 * States of the [FxaAccountManager].
 */
enum class AccountState {
    Start,
    NotAuthenticated,
    AuthenticationProblem,
    AuthenticatedNoProfile,
    AuthenticatedWithProfile,
}

/**
 * Base class for [FxaAccountManager] state machine events.
 * Events aren't a simple enum class because we might want to pass data along with some of the events.
 */
internal sealed class Event {
    override fun toString(): String {
        // For a better logcat experience.
        return this.javaClass.simpleName
    }

    internal object Init : Event()

    object AccountNotFound : Event()
    object AccountRestored : Event()

    object Authenticate : Event()
    data class Authenticated(val authData: FxaAuthData) : Event() {
        override fun toString(): String {
            // data classes define their own toString, so we override it here as well as in the base
            // class to avoid exposing 'code' and 'state' in logs.
            return this.javaClass.simpleName
        }
    }
    data class AuthenticationError(val error: AuthException) : Event() {
        override fun toString(): String {
            return this.javaClass.simpleName
        }
    }
    data class SignInShareableAccount(val account: ShareableAccount) : Event() {
        override fun toString(): String {
            return this.javaClass.simpleName
        }
    }
    object SignedInShareableAccount : Event()
    object RecoveredFromAuthenticationProblem : Event()
    object FetchProfile : Event()
    object FetchedProfile : Event()

    object FailedToAuthenticate : Event()
    object FailedToFetchProfile : Event()

    object Logout : Event()

    data class Pair(val pairingUrl: String) : Event() {
        override fun toString(): String {
            // data classes define their own toString, so we override it here as well as in the base
            // class to avoid exposing the 'pairingUrl' in logs.
            return this.javaClass.simpleName
        }
    }
}
