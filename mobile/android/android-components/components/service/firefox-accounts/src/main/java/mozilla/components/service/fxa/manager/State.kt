/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa.manager

import mozilla.components.service.fxa.FxaAuthData
import mozilla.components.service.fxa.sharing.ShareableAccount

/**
 * States of the [FxaAccountManager].
 */
enum class AccountState {
    Start,
    NotAuthenticated,
    AuthenticationProblem,
    CanAutoRetryAuthenticationViaTokenCopy,
    CanAutoRetryAuthenticationViaTokenReuse,
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

    /**
     * Fired during account init, if an in-flight copy migration was detected.
     */
    object InFlightCopyMigration : Event()

    /**
     * Fired during account init, if an in-flight copy migration was detected.
     */
    object InFlightReuseMigration : Event()

    data class AuthenticationError(val operation: String, val errorCountWithinTheTimeWindow: Int = 1) : Event() {
        override fun toString(): String {
            return "${this.javaClass.simpleName} - $operation"
        }
    }
    data class SignInShareableAccount(val account: ShareableAccount, val reuseAccount: Boolean) : Event() {
        override fun toString(): String {
            return this.javaClass.simpleName
        }
    }
    data class SignedInShareableAccount(val reuseAccount: Boolean) : Event()

    /**
     * Fired during SignInShareableAccount(reuseAccount=true) processing if an intermittent problem is encountered.
     */
    object RetryLaterViaTokenReuse : Event()

    /**
     * Fired during SignInShareableAccount(reuseAccount=false) processing if an intermittent problem is encountered.
     */
    object RetryLaterViaTokenCopy : Event()

    /**
     * Fired to trigger a migration retry.
     */
    object RetryMigration : Event()

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

internal object FxaStateMatrix {
    /**
     * State transition matrix.
     * @return An optional [AccountState] if provided state+event combination results in a
     * state transition. Note that states may transition into themselves.
     */
    internal fun nextState(state: AccountState, event: Event): AccountState? =
        when (state) {
            AccountState.Start -> when (event) {
                Event.Init -> AccountState.Start
                Event.AccountNotFound -> AccountState.NotAuthenticated
                Event.AccountRestored -> AccountState.AuthenticatedNoProfile

                Event.InFlightCopyMigration -> AccountState.CanAutoRetryAuthenticationViaTokenCopy
                Event.InFlightReuseMigration -> AccountState.CanAutoRetryAuthenticationViaTokenReuse

                else -> null
            }
            AccountState.NotAuthenticated -> when (event) {
                Event.Authenticate -> AccountState.NotAuthenticated
                Event.FailedToAuthenticate -> AccountState.NotAuthenticated

                is Event.SignInShareableAccount -> AccountState.NotAuthenticated
                is Event.SignedInShareableAccount -> AccountState.AuthenticatedNoProfile
                is Event.RetryLaterViaTokenCopy -> AccountState.CanAutoRetryAuthenticationViaTokenCopy
                is Event.RetryLaterViaTokenReuse -> AccountState.CanAutoRetryAuthenticationViaTokenReuse

                is Event.Pair -> AccountState.NotAuthenticated
                is Event.Authenticated -> AccountState.AuthenticatedNoProfile
                else -> null
            }
            AccountState.CanAutoRetryAuthenticationViaTokenCopy -> when (event) {
                Event.RetryMigration -> AccountState.CanAutoRetryAuthenticationViaTokenCopy
                Event.FailedToAuthenticate -> AccountState.NotAuthenticated
                is Event.SignedInShareableAccount -> AccountState.AuthenticatedNoProfile
                Event.RetryLaterViaTokenCopy -> AccountState.CanAutoRetryAuthenticationViaTokenCopy
                Event.Logout -> AccountState.NotAuthenticated
                else -> null
            }
            AccountState.CanAutoRetryAuthenticationViaTokenReuse -> when (event) {
                Event.RetryMigration -> AccountState.CanAutoRetryAuthenticationViaTokenReuse
                Event.FailedToAuthenticate -> AccountState.NotAuthenticated
                is Event.SignedInShareableAccount -> AccountState.AuthenticatedNoProfile
                Event.RetryLaterViaTokenReuse -> AccountState.CanAutoRetryAuthenticationViaTokenReuse
                Event.Logout -> AccountState.NotAuthenticated
                else -> null
            }
            AccountState.AuthenticatedNoProfile -> when (event) {
                is Event.AuthenticationError -> AccountState.AuthenticationProblem
                Event.FetchProfile -> AccountState.AuthenticatedNoProfile
                Event.FetchedProfile -> AccountState.AuthenticatedWithProfile
                Event.FailedToFetchProfile -> AccountState.AuthenticatedNoProfile
                Event.Logout -> AccountState.NotAuthenticated
                else -> null
            }
            AccountState.AuthenticatedWithProfile -> when (event) {
                is Event.AuthenticationError -> AccountState.AuthenticationProblem
                Event.Logout -> AccountState.NotAuthenticated
                else -> null
            }
            AccountState.AuthenticationProblem -> when (event) {
                Event.Authenticate -> AccountState.AuthenticationProblem
                Event.FailedToAuthenticate -> AccountState.AuthenticationProblem
                Event.RecoveredFromAuthenticationProblem -> AccountState.AuthenticatedNoProfile
                is Event.Authenticated -> AccountState.AuthenticatedNoProfile
                Event.Logout -> AccountState.NotAuthenticated
                else -> null
            }
        }
}
