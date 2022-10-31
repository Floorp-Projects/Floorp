/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa.manager

import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Test

class StateKtTest {
    private fun assertNextStateForStateEventPair(state: State, event: Event, nextState: State?) {
        val expectedNextState = when (state) {
            is State.Idle -> when (state.accountState) {
                AccountState.NotAuthenticated -> when (event) {
                    Event.Account.Start -> State.Active(ProgressState.Initializing)
                    Event.Account.BeginEmailFlow -> State.Active(ProgressState.BeginningAuthentication)
                    is Event.Account.BeginPairingFlow -> State.Active(ProgressState.BeginningAuthentication)
                    is Event.Account.MigrateFromAccount -> State.Active(ProgressState.MigratingAccount)
                    else -> null
                }
                AccountState.IncompleteMigration -> when (event) {
                    Event.Account.RetryMigration -> State.Active(ProgressState.MigratingAccount)
                    Event.Account.Logout -> State.Active(ProgressState.LoggingOut)
                    else -> null
                }
                AccountState.Authenticated -> when (event) {
                    is Event.Account.AuthenticationError -> State.Active(ProgressState.RecoveringFromAuthProblem)
                    Event.Account.AccessTokenKeyError -> State.Idle(AccountState.AuthenticationProblem)
                    Event.Account.Logout -> State.Active(ProgressState.LoggingOut)
                    else -> null
                }
                AccountState.AuthenticationProblem -> when (event) {
                    Event.Account.BeginEmailFlow -> State.Active(ProgressState.BeginningAuthentication)
                    Event.Account.Logout -> State.Active(ProgressState.LoggingOut)
                    else -> null
                }
            }
            is State.Active -> when (state.progressState) {
                ProgressState.Initializing -> when (event) {
                    Event.Progress.AccountNotFound -> State.Idle(AccountState.NotAuthenticated)
                    Event.Progress.AccountRestored -> State.Active(ProgressState.CompletingAuthentication)
                    is Event.Progress.IncompleteMigration -> State.Active(ProgressState.MigratingAccount)
                    else -> null
                }
                ProgressState.BeginningAuthentication -> when (event) {
                    Event.Progress.FailedToBeginAuth -> State.Idle(AccountState.NotAuthenticated)
                    Event.Progress.CancelAuth -> State.Idle(AccountState.NotAuthenticated)
                    is Event.Progress.AuthData -> State.Active(ProgressState.CompletingAuthentication)
                    else -> null
                }
                ProgressState.CompletingAuthentication -> when (event) {
                    Event.Progress.FailedToCompleteAuth -> State.Idle(AccountState.NotAuthenticated)
                    Event.Progress.FailedToCompleteAuthRestore -> State.Idle(AccountState.NotAuthenticated)
                    is Event.Progress.CompletedAuthentication -> State.Idle(AccountState.Authenticated)
                    else -> null
                }
                ProgressState.MigratingAccount -> when (event) {
                    Event.Progress.FailedToCompleteMigration -> State.Idle(AccountState.NotAuthenticated)
                    is Event.Progress.Migrated -> State.Active(ProgressState.CompletingAuthentication)
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

        assertEquals(expectedNextState, nextState)
    }

    private fun instantiateEvent(eventClassSimpleName: String): Event {
        return when (eventClassSimpleName) {
            "Start" -> Event.Account.Start
            "BeginPairingFlow" -> Event.Account.BeginPairingFlow("http://some.pairing.url.com")
            "BeginEmailFlow" -> Event.Account.BeginEmailFlow
            "CancelAuth" -> Event.Progress.CancelAuth
            "AuthenticationError" -> Event.Account.AuthenticationError("fxa op")
            "AccessTokenKeyError" -> Event.Account.AccessTokenKeyError
            "MigrateFromAccount" -> Event.Account.MigrateFromAccount(mock(), true)
            "RetryMigration" -> Event.Account.RetryMigration
            "Logout" -> Event.Account.Logout
            "AccountNotFound" -> Event.Progress.AccountNotFound
            "AccountRestored" -> Event.Progress.AccountRestored
            "AuthData" -> Event.Progress.AuthData(mock())
            "IncompleteMigration" -> Event.Progress.IncompleteMigration(true)
            "Migrated" -> Event.Progress.Migrated(true)
            "LoggedOut" -> Event.Progress.LoggedOut
            "FailedToRecoverFromAuthenticationProblem" -> Event.Progress.FailedToRecoverFromAuthenticationProblem
            "RecoveredFromAuthenticationProblem" -> Event.Progress.RecoveredFromAuthenticationProblem
            "CompletedAuthentication" -> Event.Progress.CompletedAuthentication(mock())
            "FailedToBeginAuth" -> Event.Progress.FailedToBeginAuth
            "FailedToCompleteAuth" -> Event.Progress.FailedToCompleteAuth
            "FailedToCompleteMigration" -> Event.Progress.FailedToCompleteMigration
            "FailedToCompleteAuthRestore" -> Event.Progress.FailedToCompleteAuthRestore
            else -> {
                throw AssertionError("Unknown event: $eventClassSimpleName")
            }
        }
    }

    @Test
    fun `state transition matrix`() {
        // We want to test every combination of state/event. Do that by iterating over entire sets.
        ProgressState.values().forEach { state ->
            Event.Progress::class.sealedSubclasses.map { instantiateEvent(it.simpleName!!) }.forEach {
                val ss = State.Active(state)
                assertNextStateForStateEventPair(
                    ss,
                    it,
                    ss.next(it),
                )
            }
        }

        AccountState.values().forEach { state ->
            Event.Account::class.sealedSubclasses.map { instantiateEvent(it.simpleName!!) }.forEach {
                val ss = State.Idle(state)
                assertNextStateForStateEventPair(
                    ss,
                    it,
                    ss.next(it),
                )
            }
        }
    }
}
