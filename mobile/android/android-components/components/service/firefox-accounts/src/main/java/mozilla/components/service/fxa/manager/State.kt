/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa.manager

import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.FxAEntryPoint
import mozilla.components.service.fxa.FxaAuthData

/**
 * This file is the "heart" of the accounts system. It's a finite state machine.
 * Described below are all of the possible states, transitions between them and events which can
 * trigger these transitions.
 *
 * There are two types of states - [State.Idle] and [State.Active]. Idle states are modeled by
 * [AccountState], while active states are modeled by [ProgressState].
 *
 * Correspondingly, there are also two types of events - [Event.Account] and [Event.Progress].
 * [Event.Account] events will be "understood" by states of type [AccountState].
 * [Event.Progress] events will be "understood" by states of type  [ProgressState].
 * The word "understood" means "may trigger a state transition".
 *
 * [Event.Account] events are also referred to as "external events", since they are sent either directly
 * by the user, or by some event of external origin (e.g. hitting an auth problem while communicating
 * with a server).
 *
 * Similarly, [Event.Progress] events are referred to as "internal events", since they are sent by
 * the internal processes within the state machine itself, e.g. as a result of some action being taken
 * as a side-effect of a state transition.
 *
 * States of type [AccountState] are "stable" states (or, idle). After reaching one of these states,
 * state machine will remain there unless an [Event.Account] is received. An example of a stable state
 * is LoggedOut. No transitions will take place unless user tries to sign-in, or system tries to restore
 * an account from disk (e.g. during startup).
 *
 * States of type [ProgressState] are "transition" state (or, active). These states represent certain processes
 * that take place - for example, an in-progress authentication, or an account recovery.
 * Once these processes are completed (either succeeding or failing), an [Event.Progress] is expected
 * to be received, triggering a state transition.
 *
 * Most states have "side-effects" - actions that happen during a transition into the state.
 * These side-effects are described in [FxaAccountManager]. For example, a side-effect of
 * [ProgressState.BeginningAuthentication] may be sending a request to an auth server to initialize an OAuth flow.
 *
 * Side-effects of [AccountState] states are simply about notifying external observers that a certain
 * stable account state was reached.
 *
 * Side-effects of [ProgressState] states are various actions we need to take to execute the transition,
 * e.g. talking to servers, serializing some state to disk, cleaning up, etc.
 *
 * State transitions are described by a transition matrix, which is described in [State.next].
 */

internal sealed class AccountState {
    object Authenticated : AccountState()
    data class Authenticating(val oAuthUrl: String) : AccountState()
    object AuthenticationProblem : AccountState()
    object NotAuthenticated : AccountState()
}

internal enum class ProgressState {
    Initializing,
    BeginningAuthentication,
    CompletingAuthentication,
    RecoveringFromAuthProblem,
    LoggingOut,
}

internal sealed class Event {
    internal sealed class Account : Event() {
        internal object Start : Account()
        data class BeginEmailFlow(val entrypoint: FxAEntryPoint) : Account()
        data class BeginPairingFlow(val pairingUrl: String?, val entrypoint: FxAEntryPoint) : Account()
        data class AuthenticationError(val operation: String, val errorCountWithinTheTimeWindow: Int = 1) : Account() {
            override fun toString(): String {
                return "${this.javaClass.simpleName} - $operation"
            }
        }
        object AccessTokenKeyError : Account()

        object Logout : Account()
    }

    internal sealed class Progress : Event() {
        object AccountNotFound : Progress()
        object AccountRestored : Progress()

        data class AuthData(val authData: FxaAuthData) : Progress()

        object FailedToBeginAuth : Progress()
        object FailedToCompleteAuthRestore : Progress()
        object FailedToCompleteAuth : Progress()

        object CancelAuth : Progress()

        object FailedToRecoverFromAuthenticationProblem : Progress()
        object RecoveredFromAuthenticationProblem : Progress()

        object LoggedOut : Progress()

        data class StartedOAuthFlow(val oAuthUrl: String) : Progress()
        data class CompletedAuthentication(val authType: AuthType) : Progress()
    }

    /**
     * Get a string to display in the breadcrumbs
     *
     * The main point of this function is to avoid using the string "auth", which gets filtered by
     * Sentry.  Use "ath" as a hacky replacement.
     */
    fun breadcrumbDisplay(): String = when (this) {
        is Account.Start -> "Account.Start"
        is Account.BeginEmailFlow -> "Account.BeginEmailFlow"
        is Account.BeginPairingFlow -> "Account.BeginPairingFlow"
        is Account.AuthenticationError -> "Account.AthenticationError($operation)"
        is Account.AccessTokenKeyError -> "Account.AccessTknKeyError"
        is Account.Logout -> "Account.Logout"
        is Progress.AccountNotFound -> "Progress.AccountNotFound"
        is Progress.AccountRestored -> "Progress.AccountRestored"
        is Progress.AuthData -> "Progress.LoggedOut"
        is Progress.FailedToBeginAuth -> "Progress.FailedToBeginAth"
        is Progress.FailedToCompleteAuthRestore -> "Progress.FailedToCompleteAthRestore"
        is Progress.FailedToCompleteAuth -> "Progress.FailedToCompleteAth"
        is Progress.CancelAuth -> "Progress.CancelAth"
        is Progress.FailedToRecoverFromAuthenticationProblem -> "Progress.FailedToRecoverFromAthenticationProblem"
        is Progress.RecoveredFromAuthenticationProblem -> "Progress.RecoveredFromAthenticationProblem"
        is Progress.LoggedOut -> "Progress.LoggedOut"
        is Progress.StartedOAuthFlow -> "Progress.StartedOAthFlow"
        is Progress.CompletedAuthentication -> "Progress.CompletedAthentication"
    }
}

internal sealed class State {
    data class Idle(val accountState: AccountState) : State()
    data class Active(val progressState: ProgressState) : State()

    /**
     * Get a string to display in the breadcrumbs
     *
     * The main point of this function is to avoid using the string "auth", which gets filtered by
     * Sentry.  Use "ath" as a hacky replacement.
     */
    fun breadcrumbDisplay(): String = when (this) {
        is Idle -> when (accountState) {
            is AccountState.Authenticated -> "AccountState.Athenticated"
            is AccountState.Authenticating -> "AccountState.Athenticating"
            is AccountState.AuthenticationProblem -> "AccountState.AthenticationProblem"
            is AccountState.NotAuthenticated -> "AccountState.NotAthenticated"
        }
        is Active -> when (progressState) {
            ProgressState.Initializing -> "ProgressState.Initializing"
            ProgressState.BeginningAuthentication -> "ProgressState.BeginningAthentication"
            ProgressState.CompletingAuthentication -> "ProgressState.CompletingAthentication"
            ProgressState.RecoveringFromAuthProblem -> "ProgressState.RecoveringFromAthProblem"
            ProgressState.LoggingOut -> "ProgressState.LoggingOut"
        }
    }
}

internal fun State.next(event: Event): State? = when (this) {
    // Reacting to external events.
    is State.Idle -> when (this.accountState) {
        AccountState.NotAuthenticated -> when (event) {
            Event.Account.Start -> State.Active(ProgressState.Initializing)
            is Event.Account.BeginEmailFlow -> State.Active(ProgressState.BeginningAuthentication)
            is Event.Account.BeginPairingFlow -> State.Active(ProgressState.BeginningAuthentication)
            else -> null
        }
        is AccountState.Authenticating -> when (event) {
            is Event.Progress.AuthData -> State.Active(ProgressState.CompletingAuthentication)
            Event.Progress.CancelAuth -> State.Idle(AccountState.NotAuthenticated)
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
            else -> null
        }
        ProgressState.BeginningAuthentication -> when (event) {
            Event.Progress.FailedToBeginAuth -> State.Idle(AccountState.NotAuthenticated)
            is Event.Progress.StartedOAuthFlow -> State.Idle(AccountState.Authenticating(event.oAuthUrl))
            else -> null
        }
        ProgressState.CompletingAuthentication -> when (event) {
            is Event.Progress.CompletedAuthentication -> State.Idle(AccountState.Authenticated)
            is Event.Account.AuthenticationError -> State.Active(ProgressState.RecoveringFromAuthProblem)
            Event.Progress.FailedToCompleteAuthRestore -> State.Idle(AccountState.NotAuthenticated)
            Event.Progress.FailedToCompleteAuth -> State.Idle(AccountState.NotAuthenticated)
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
