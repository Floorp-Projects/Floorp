/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins

import androidx.lifecycle.LifecycleOwner
import androidx.navigation.NavController
import androidx.preference.Preference
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.manager.SyncEnginesStorage
import org.mozilla.fenix.R
import org.mozilla.fenix.settings.logins.fragment.SavedLoginsAuthFragmentDirections

/**
 * Helper to manage the [R.string.pref_key_password_sync_logins] preference.
 */
class SyncLoginsPreferenceView(
    private val syncLoginsPreference: Preference,
    lifecycleOwner: LifecycleOwner,
    accountManager: FxaAccountManager,
    private val navController: NavController
) {

    init {
        accountManager.register(object : AccountObserver {
            override fun onAuthenticated(account: OAuthAccount, authType: AuthType) =
                updateSyncPreferenceStatus()
            override fun onLoggedOut() = updateSyncPreferenceNeedsLogin()
            override fun onAuthenticationProblems() = updateSyncPreferenceNeedsReauth()
        }, owner = lifecycleOwner)

        val accountExists = accountManager.authenticatedAccount() != null
        val needsReauth = accountManager.accountNeedsReauth()
        when {
            needsReauth -> updateSyncPreferenceNeedsReauth()
            accountExists -> updateSyncPreferenceStatus()
            !accountExists -> updateSyncPreferenceNeedsLogin()
        }
    }

    /**
     * Show the current status of the sync preference (on/off) for the logged in user.
     */
    private fun updateSyncPreferenceStatus() {
        syncLoginsPreference.apply {
            val syncEnginesStatus = SyncEnginesStorage(context).getStatus()
            val loginsSyncStatus = syncEnginesStatus.getOrElse(SyncEngine.Passwords) { false }
            summary = context.getString(
                if (loginsSyncStatus) R.string.preferences_passwords_sync_logins_on
                else R.string.preferences_passwords_sync_logins_off
            )
            setOnPreferenceClickListener {
                navigateToAccountSettingsFragment()
                true
            }
        }
    }

    /**
     * Indicate that the user can sign in to turn on sync.
     */
    private fun updateSyncPreferenceNeedsLogin() {
        syncLoginsPreference.apply {
            summary = context.getString(R.string.preferences_passwords_sync_logins_sign_in)
            setOnPreferenceClickListener {
                navigateToTurnOnSyncFragment()
                true
            }
        }
    }

    /**
     * Indicate that the user can fix their account problems to turn on sync.
     */
    private fun updateSyncPreferenceNeedsReauth() {
        syncLoginsPreference.apply {
            summary = context.getString(R.string.preferences_passwords_sync_logins_reconnect)
            setOnPreferenceClickListener {
                navigateToAccountProblemFragment()
                true
            }
        }
    }

    private fun navigateToAccountSettingsFragment() {
        val directions =
            SavedLoginsAuthFragmentDirections.actionGlobalAccountSettingsFragment()
        navController.navigate(directions)
    }

    private fun navigateToAccountProblemFragment() {
        val directions = SavedLoginsAuthFragmentDirections.actionGlobalAccountProblemFragment()
        navController.navigate(directions)
    }

    private fun navigateToTurnOnSyncFragment() {
        val directions = SavedLoginsAuthFragmentDirections.actionSavedLoginsAuthFragmentToTurnOnSyncFragment()
        navController.navigate(directions)
    }
}
