/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import android.content.Context
import androidx.annotation.VisibleForTesting
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.Profile
import org.mozilla.fenix.ext.components

/**
 * Observes various account-related events and dispatches the [OnboardingState] based on whether
 * or not the account is authenticated.
 */
class OnboardingAccountObserver(
    private val context: Context,
    private val dispatchChanges: (state: OnboardingState) -> Unit,
) : AccountObserver {

    private val accountManager by lazy { context.components.backgroundServices.accountManager }

    /**
     * Returns the current [OnboardingState] based on the account state.
     */
    fun getOnboardingState(): OnboardingState {
        val account = accountManager.authenticatedAccount()

        return if (account != null) {
            OnboardingState.SignedIn
        } else {
            OnboardingState.SignedOutNoAutoSignIn
        }
    }

    @VisibleForTesting
    internal fun emitChanges() {
        dispatchChanges(getOnboardingState())
    }

    override fun onAuthenticated(account: OAuthAccount, authType: AuthType) = emitChanges()
    override fun onAuthenticationProblems() = emitChanges()
    override fun onLoggedOut() = emitChanges()
    override fun onProfileUpdated(profile: Profile) = emitChanges()
}
