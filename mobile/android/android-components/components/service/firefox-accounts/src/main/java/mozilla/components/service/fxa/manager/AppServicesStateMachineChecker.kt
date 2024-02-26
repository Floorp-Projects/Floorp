/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa.manager

import mozilla.appservices.fxaclient.FxaEvent
import mozilla.appservices.fxaclient.FxaRustAuthState
import mozilla.appservices.fxaclient.FxaState
import mozilla.appservices.fxaclient.FxaStateCheckerEvent
import mozilla.appservices.fxaclient.FxaStateCheckerState
import mozilla.appservices.fxaclient.FxaStateMachineChecker
import mozilla.components.concept.sync.DeviceConfig
import mozilla.components.service.fxa.into
import mozilla.appservices.fxaclient.DeviceConfig as ASDeviceConfig

/**
 * Checks the new app-services state machine logic against the current android-components code
 *
 * This is a temporary measure to prep for migrating the android-components code to using the new
 * app-services state machine. It performs a "dry-run" test of the code where we check the new logic
 * against the current logic, without performing any side-effects.
 *
 * If one of the checks fails, then we report an error to Sentry.  After that the calls become
 * no-ops to avoid spamming sentry with errors from a single client.  By checking the Sentry errors,
 * we can find places where the application-services logic doesn't match the current
 * android-components logic and fix the issue.
 *
 * Once we determine that the new application-services code is correct, let's switch the
 * android-components code to using it (https://bugzilla.mozilla.org/show_bug.cgi?id=1867793) and
 * delete all this code.
 */
object AppServicesStateMachineChecker {
    /**
     * The Rust state machine checker.  This handles the actual checks and sentry reporting.
     */
    private val rustChecker = FxaStateMachineChecker()

    /**
     * Handle an event about to be processed
     *
     * Call this when processing an android-components event, after checking that it's valid.
     */
    internal fun handleEvent(event: Event, deviceConfig: DeviceConfig, scopes: Set<String>) {
        val convertedEvent = when (event) {
            Event.Account.Start -> FxaEvent.Initialize(
                ASDeviceConfig(
                    name = deviceConfig.name,
                    deviceType = deviceConfig.type.into(),
                    capabilities = ArrayList(deviceConfig.capabilities.map { it.into() }),
                ),
            )
            is Event.Account.BeginEmailFlow -> FxaEvent.BeginOAuthFlow(ArrayList(scopes), event.entrypoint.entryName)
            is Event.Account.BeginPairingFlow -> {
                // pairingUrl should always be non-null, if it is somehow null let's use a
                // placeholder value that can be identified when checking in sentry
                val pairingUrl = event.pairingUrl ?: "<null>"
                FxaEvent.BeginPairingFlow(pairingUrl, ArrayList(scopes), event.entrypoint.entryName)
            }
            is Event.Account.AuthenticationError -> {
                // There are basically 2 ways for this to happen:
                //
                // - Another component called `FxaAccountManager.encounteredAuthError()`.  In this
                //   case, we should initiate the state transition by sending the state machine the
                //   `FxaEvent.CheckAuthorizationStatus`
                // - `FxaAccountManager` sent it to itself, because there was an error when
                //   `internalStateSideEffects` called `finalizeDevice()`.  In this case, we're
                //   already in the middle of a state transition and already sent the state machine
                //   the `EnsureCapabilitiesAuthError` event, so we should ignore it.
                if (event.operation == "finalizeDevice") {
                    return
                } else {
                    FxaEvent.CheckAuthorizationStatus
                }
            }
            Event.Account.AccessTokenKeyError -> FxaEvent.CheckAuthorizationStatus
            Event.Account.Logout -> FxaEvent.Disconnect
            // This is the one ProgressEvent that's considered a "public event" in app-services
            is Event.Progress.AuthData -> FxaEvent.CompleteOAuthFlow(event.authData.code, event.authData.state)
            is Event.Progress.CancelAuth -> FxaEvent.CancelOAuthFlow
            else -> return
        }
        rustChecker.handlePublicEvent(convertedEvent)
    }

    /**
     * Check a new account state
     *
     * Call this after transitioning to an android-components account state.
     */
    internal fun checkAccountState(state: AccountState) {
        val convertedState = when (state) {
            AccountState.NotAuthenticated -> FxaState.Disconnected
            is AccountState.Authenticating -> FxaState.Authenticating(state.oAuthUrl)
            AccountState.Authenticated -> FxaState.Connected
            AccountState.AuthenticationProblem -> FxaState.AuthIssues
        }
        rustChecker.checkPublicState(convertedState)
    }

    /**
     * General validation for new progress state being processed by the AC state machine.
     *
     * This handles all validation for most state transitions in a simple manner. The one transition
     * it can't handle is completing oauth, which entails multiple FxA calls and can fail in multiple
     * different ways.  For that, the lower-level `checkInternalState` and `handleInternalEvent` are
     * used.
     */
    @Suppress("LongMethod")
    internal fun validateProgressEvent(progressEvent: Event.Progress, via: Event, scopes: Set<String>) {
        when (progressEvent) {
            Event.Progress.AccountRestored -> {
                AppServicesStateMachineChecker.checkInternalState(FxaStateCheckerState.GetAuthState)
                AppServicesStateMachineChecker.handleInternalEvent(
                    FxaStateCheckerEvent.GetAuthStateSuccess(FxaRustAuthState.CONNECTED),
                )
            }
            Event.Progress.AccountNotFound -> {
                AppServicesStateMachineChecker.checkInternalState(FxaStateCheckerState.GetAuthState)
                AppServicesStateMachineChecker.handleInternalEvent(
                    FxaStateCheckerEvent.GetAuthStateSuccess(FxaRustAuthState.DISCONNECTED),
                )
            }
            is Event.Progress.StartedOAuthFlow -> {
                when (via) {
                    is Event.Account.BeginEmailFlow -> {
                        AppServicesStateMachineChecker.checkInternalState(
                            FxaStateCheckerState.BeginOAuthFlow(ArrayList(scopes), via.entrypoint.entryName),
                        )
                        AppServicesStateMachineChecker.handleInternalEvent(
                            FxaStateCheckerEvent.BeginOAuthFlowSuccess(progressEvent.oAuthUrl),
                        )
                    }
                    is Event.Account.BeginPairingFlow -> {
                        AppServicesStateMachineChecker.checkInternalState(
                            FxaStateCheckerState.BeginPairingFlow(
                                via.pairingUrl!!,
                                ArrayList(scopes),
                                via.entrypoint.entryName,
                            ),
                        )
                        AppServicesStateMachineChecker.handleInternalEvent(
                            FxaStateCheckerEvent.BeginPairingFlowSuccess(progressEvent.oAuthUrl),
                        )
                    }
                    // This branch should never be taken, if it does we'll probably see a state
                    // check error down the line.
                    else -> Unit
                }
            }
            Event.Progress.FailedToBeginAuth -> {
                when (via) {
                    is Event.Account.BeginEmailFlow -> {
                        AppServicesStateMachineChecker.checkInternalState(
                            FxaStateCheckerState.BeginOAuthFlow(ArrayList(scopes), via.entrypoint.entryName),
                        )
                    }
                    is Event.Account.BeginPairingFlow -> {
                        AppServicesStateMachineChecker.checkInternalState(
                            FxaStateCheckerState.BeginPairingFlow(
                                via.pairingUrl!!,
                                ArrayList(scopes),
                                via.entrypoint.entryName,
                            ),
                        )
                    }
                    // This branch should never be taken, if it does we'll probably see a state
                    // check error down the line.
                    else -> Unit
                }
                AppServicesStateMachineChecker.handleInternalEvent(FxaStateCheckerEvent.CallError)
            }
            Event.Progress.LoggedOut -> {
                AppServicesStateMachineChecker.checkInternalState(
                    FxaStateCheckerState.Disconnect,
                )
                AppServicesStateMachineChecker.handleInternalEvent(
                    FxaStateCheckerEvent.DisconnectSuccess,
                )
            }
            Event.Progress.RecoveredFromAuthenticationProblem -> {
                AppServicesStateMachineChecker.checkInternalState(FxaStateCheckerState.CheckAuthorizationStatus)
                AppServicesStateMachineChecker.handleInternalEvent(
                    FxaStateCheckerEvent.CheckAuthorizationStatusSuccess(true),
                )
            }
            Event.Progress.FailedToRecoverFromAuthenticationProblem -> {
                // `via` should always be `AuthenticationError` if not, we'll probably see a state
                // check error down the line.
                if (via is Event.Account.AuthenticationError) {
                    if (via.errorCountWithinTheTimeWindow >= AUTH_CHECK_CIRCUIT_BREAKER_COUNT) {
                        // In this case, the state machine fails early and doesn't actualy make any
                        // calls
                        return
                    }
                    AppServicesStateMachineChecker.checkInternalState(FxaStateCheckerState.CheckAuthorizationStatus)
                    AppServicesStateMachineChecker.handleInternalEvent(
                        FxaStateCheckerEvent.CheckAuthorizationStatusSuccess(false),
                    )
                }
            }
            else -> Unit
        }
    }

    /**
     * Check an app-services internal state
     *
     * The app-services internal states correspond to internal firefox account method calls.  Call
     * this before making one of those calls.
     */
    internal fun checkInternalState(state: FxaStateCheckerState) {
        rustChecker.checkInternalState(state)
    }

    /**
     * Handle an app-services internal event
     *
     * The app-services internal states correspond the results of internal firefox account method
     * calls.  Call this before after making a call.
     */
    internal fun handleInternalEvent(event: FxaStateCheckerEvent) {
        rustChecker.handleInternalEvent(event)
    }
}
