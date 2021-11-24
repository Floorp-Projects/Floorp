/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa.manager

import mozilla.components.concept.sync.AuthType
import mozilla.components.service.fxa.FxaAuthData
import mozilla.components.service.fxa.sharing.ShareableAccount

internal enum class AccountState {
    NotAuthenticated,
    IncompleteMigration,
    Authenticated,
    AuthenticationProblem
}

internal enum class ProgressState {
    Initializing,
    BeginningAuthentication,
    CompletingAuthentication,
    MigratingAccount,
    RecoveringFromAuthProblem,
    LoggingOut,
}

internal sealed class Event {
    internal sealed class Account : Event() {
        internal object Start : Account()
        object BeginEmailFlow : Account()
        data class BeginPairingFlow(val pairingUrl: String?) : Account()
        data class AuthenticationError(val operation: String, val errorCountWithinTheTimeWindow: Int = 1) : Account() {
            override fun toString(): String {
                return "${this.javaClass.simpleName} - $operation"
            }
        }
        object AccessTokenKeyError : Account()

        data class MigrateFromAccount(val account: ShareableAccount, val reuseSessionToken: Boolean) : Account() {
            override fun toString(): String {
                return this.javaClass.simpleName
            }
        }

        object RetryMigration : Account()

        object Logout : Account()
    }

    internal sealed class Progress : Event() {
        object AccountNotFound : Progress()
        object AccountRestored : Progress()
        data class IncompleteMigration(val reuseSessionToken: Boolean) : Progress()

        data class AuthData(val authData: FxaAuthData) : Progress()
        data class Migrated(val reusedSessionToken: Boolean) : Progress()

        object FailedToCompleteMigration : Progress()
        object FailedToBeginAuth : Progress()
        object FailedToCompleteAuthRestore : Progress()
        object FailedToCompleteAuth : Progress()

        object CancelAuth : Progress()

        object FailedToRecoverFromAuthenticationProblem : Progress()
        object RecoveredFromAuthenticationProblem : Progress()

        object LoggedOut : Progress()

        data class CompletedAuthentication(val authType: AuthType) : Progress()
    }
}

internal sealed class State {
    data class Idle(val accountState: AccountState) : State()
    data class Active(val progressState: ProgressState) : State()
}

internal fun State.next(event: Event): State? = when (this) {
    // Reacting to external events.
    is State.Idle -> when (this.accountState) {
        AccountState.NotAuthenticated -> when (event) {
            Event.Account.Start -> State.Active(ProgressState.Initializing)
            Event.Account.BeginEmailFlow -> State.Active(ProgressState.BeginningAuthentication)
            is Event.Account.BeginPairingFlow -> State.Active(ProgressState.BeginningAuthentication)
            is Event.Account.MigrateFromAccount -> State.Active(ProgressState.MigratingAccount)
            else -> null
        }
        AccountState.IncompleteMigration -> when (event) {
            is Event.Account.RetryMigration -> State.Active(ProgressState.MigratingAccount)
            is Event.Account.Logout -> State.Active(ProgressState.LoggingOut)
            else -> null
        }
        AccountState.Authenticated -> when (event) {
            is Event.Account.AuthenticationError -> State.Active(ProgressState.RecoveringFromAuthProblem)
            is Event.Account.AccessTokenKeyError -> State.Idle(AccountState.AuthenticationProblem)
            is Event.Account.Logout -> State.Active(ProgressState.LoggingOut)
            else -> null
        }
        AccountState.AuthenticationProblem -> when (event) {
            is Event.Account.Logout -> State.Active(ProgressState.LoggingOut)
            is Event.Account.BeginEmailFlow -> State.Active(ProgressState.BeginningAuthentication)
            else -> null
        }
    }
    // Reacting to internal events.
    is State.Active -> when (this.progressState) {
        ProgressState.Initializing -> when (event) {
            Event.Progress.AccountNotFound -> State.Idle(AccountState.NotAuthenticated)
            Event.Progress.AccountRestored -> State.Active(ProgressState.CompletingAuthentication)
            is Event.Progress.IncompleteMigration -> State.Active(ProgressState.MigratingAccount)
            else -> null
        }
        ProgressState.BeginningAuthentication -> when (event) {
            is Event.Progress.AuthData -> State.Active(ProgressState.CompletingAuthentication)
            Event.Progress.FailedToBeginAuth -> State.Idle(AccountState.NotAuthenticated)
            Event.Progress.CancelAuth -> State.Idle(AccountState.NotAuthenticated)
            else -> null
        }
        ProgressState.CompletingAuthentication -> when (event) {
            is Event.Progress.CompletedAuthentication -> State.Idle(AccountState.Authenticated)
            is Event.Account.AuthenticationError -> State.Active(ProgressState.RecoveringFromAuthProblem)
            Event.Progress.FailedToCompleteAuthRestore -> State.Idle(AccountState.NotAuthenticated)
            Event.Progress.FailedToCompleteAuth -> State.Idle(AccountState.NotAuthenticated)
            else -> null
        }
        ProgressState.MigratingAccount -> when (event) {
            is Event.Progress.Migrated -> State.Active(ProgressState.CompletingAuthentication)
            Event.Progress.FailedToCompleteMigration -> State.Idle(AccountState.NotAuthenticated)
            is Event.Progress.IncompleteMigration -> State.Idle(AccountState.IncompleteMigration)
            else -> null
        }
        ProgressState.RecoveringFromAuthProblem -> when (event) {
            Event.Progress.RecoveredFromAuthenticationProblem -> State.Idle(AccountState.Authenticated)
            Event.Progress.FailedToRecoverFromAuthenticationProblem -> State.Idle(AccountState.AuthenticationProblem)
            else -> null
        }
        ProgressState.LoggingOut -> when (event) {
            Event.Progress.LoggedOut -> State.Idle(AccountState.NotAuthenticated)
            else -> null
        }
    }
}
