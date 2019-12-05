/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.content.Context
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.Profile
import mozilla.components.service.fxa.sharing.ShareableAccount
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.onboarding.FenixOnboarding

/**
 * Describes various states of the home fragment UI.
 */
sealed class Mode {
    object Normal : Mode()
    object Private : Mode()
    data class Onboarding(val state: OnboardingState) : Mode()

    companion object {
        fun fromBrowsingMode(browsingMode: BrowsingMode) = when (browsingMode) {
            BrowsingMode.Normal -> Normal
            BrowsingMode.Private -> Private
        }
    }
}

/**
 * Describes various onboarding states.
 */
sealed class OnboardingState {
    // Signed out, without an option to auto-login using a shared FxA account.
    object SignedOutNoAutoSignIn : OnboardingState()
    // Signed out, with an option to auto-login into a shared FxA account.
    data class SignedOutCanAutoSignIn(val withAccount: ShareableAccount) : OnboardingState()
    // Signed in.
    object SignedIn : OnboardingState()
}

class CurrentMode(
    private val context: Context,
    private val onboarding: FenixOnboarding,
    private val browsingModeManager: BrowsingModeManager,
    private val dispatchModeChanges: (mode: Mode) -> Unit
) : AccountObserver {

    private val accountManager = context.components.backgroundServices.accountManager

    fun getCurrentMode() = if (onboarding.userHasBeenOnboarded()) {
        Mode.fromBrowsingMode(browsingModeManager.mode)
    } else {
        val account = accountManager.authenticatedAccount()
        if (account != null) {
            Mode.Onboarding(OnboardingState.SignedIn)
        } else {
            val availableAccounts = accountManager.shareableAccounts(context)
            if (availableAccounts.isEmpty()) {
                Mode.Onboarding(OnboardingState.SignedOutNoAutoSignIn)
            } else {
                Mode.Onboarding(OnboardingState.SignedOutCanAutoSignIn(availableAccounts[0]))
            }
        }
    }

    fun emitModeChanges() {
        dispatchModeChanges(getCurrentMode())
    }

    override fun onAuthenticated(account: OAuthAccount, authType: AuthType) = emitModeChanges()
    override fun onAuthenticationProblems() = emitModeChanges()
    override fun onLoggedOut() = emitModeChanges()
    override fun onProfileUpdated(profile: Profile) = emitModeChanges()
}
