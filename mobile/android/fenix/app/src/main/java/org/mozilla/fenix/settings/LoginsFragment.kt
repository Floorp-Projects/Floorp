/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.navigation.fragment.findNavController
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.service.fxa.manager.SyncEnginesStorage
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.ext.requireComponents

@Suppress("TooManyFunctions")
class LoginsFragment : PreferenceFragmentCompat(), AccountObserver {
    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.logins_preferences, rootKey)
    }

    override fun onResume() {
        super.onResume()
        activity?.title = getString(R.string.preferences_passwords_logins_and_passwords)
        (activity as AppCompatActivity).supportActionBar?.show()

        val savedLoginsKey = getPreferenceKey(R.string.pref_key_saved_logins)
        findPreference<Preference>(savedLoginsKey)?.setOnPreferenceClickListener {
            navigateToLoginsSettingsFragment()
            true
        }

        val accountManager = requireComponents.backgroundServices.accountManager
        accountManager.register(this, owner = this)

        val accountExists = accountManager.authenticatedAccount() != null
        val needsReauth = accountManager.accountNeedsReauth()
        when {
            needsReauth -> updateSyncPreferenceNeedsReauth()
            accountExists -> updateSyncPreferenceStatus()
            !accountExists -> updateSyncPreferenceNeedsLogin()
        }
    }

    override fun onAuthenticated(account: OAuthAccount, authType: AuthType) =
        updateSyncPreferenceStatus()

    override fun onLoggedOut() = updateSyncPreferenceNeedsLogin()

    override fun onAuthenticationProblems() = updateSyncPreferenceNeedsReauth()

    private fun updateSyncPreferenceStatus() {
        val syncLogins = getPreferenceKey(R.string.pref_key_password_sync_logins)
        findPreference<Preference>(syncLogins)?.apply {
            val syncEnginesStatus = SyncEnginesStorage(context!!).getStatus()
            val loginsSyncStatus = syncEnginesStatus.getOrElse(SyncEngine.Passwords) { false }
            summary = getString(
                if (loginsSyncStatus) R.string.preferences_passwords_sync_logins_on
                else R.string.preferences_passwords_sync_logins_off
            )
            setOnPreferenceClickListener {
                navigateToAccountSettingsFragment()
                true
            }
        }
    }

    private fun updateSyncPreferenceNeedsLogin() {
        val syncLogins = getPreferenceKey(R.string.pref_key_password_sync_logins)
        findPreference<Preference>(syncLogins)?.apply {
            summary = getString(R.string.preferences_passwords_sync_logins_sign_in)
            setOnPreferenceClickListener {
                navigateToTurnOnSyncFragment()
                true
            }
        }
    }

    private fun updateSyncPreferenceNeedsReauth() {
        val syncLogins = getPreferenceKey(R.string.pref_key_password_sync_logins)
        findPreference<Preference>(syncLogins)?.apply {
            summary = getString(R.string.preferences_passwords_sync_logins_reconnect)
            setOnPreferenceClickListener {
                navigateToAccountProblemFragment()
                true
            }
        }
    }

    private fun navigateToLoginsSettingsFragment() {
        val directions = LoginsFragmentDirections.actionLoginsFragmentToSavedLoginsFragment()
        findNavController().navigate(directions)
    }

    private fun navigateToAccountSettingsFragment() {
        val directions = LoginsFragmentDirections.actionLoginsFragmentToAccountSettingsFragment()
        findNavController().navigate(directions)
    }

    private fun navigateToAccountProblemFragment() {
        val directions = LoginsFragmentDirections.actionLoginsFragmentToAccountProblemFragment()
        findNavController().navigate(directions)
    }

    private fun navigateToTurnOnSyncFragment() {
        val directions = LoginsFragmentDirections.actionLoginsFragmentToTurnOnSyncFragment()
        findNavController().navigate(directions)
    }
}
