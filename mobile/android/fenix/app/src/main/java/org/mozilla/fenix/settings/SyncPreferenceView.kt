/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import androidx.lifecycle.LifecycleOwner
import androidx.preference.Preference
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.manager.SyncEnginesStorage
import org.mozilla.fenix.R

/**
 * A view to help manage the sync preference in the "Logins and passwords" and "Credit cards"
 * settings. The provided [syncPreference] is used to navigate to the different fragments
 * that manages the sync account authentication. A summary status will be also added
 * depending on the sync account status.
 *
 * @param syncPreference The sync [Preference] to update and handle navigation.
 * @param lifecycleOwner View lifecycle owner used to determine when to cancel UI jobs.
 * @param accountManager An instance of [FxaAccountManager].
 * @param syncEngine The sync engine that will be used for the sync status lookup.
 * @param onSignInToSyncClicked A callback executed when the [syncPreference] is clicked with a
 * preference status of "Sign in to Sync".
 * @param onSyncStatusClicked A callback executed when the [syncPreference] is clicked with a
 * preference status of "On" or "Off".
 * @param onReconnectClicked A callback executed when the [syncPreference] is clicked with a
 * preference status of "Reconnect".
 */
@Suppress("LongParameterList")
class SyncPreferenceView(
    private val syncPreference: Preference,
    lifecycleOwner: LifecycleOwner,
    accountManager: FxaAccountManager,
    private val syncEngine: SyncEngine,
    private val onSignInToSyncClicked: () -> Unit = {},
    private val onSyncStatusClicked: () -> Unit = {},
    private val onReconnectClicked: () -> Unit = {}
) {

    init {
        accountManager.register(object : AccountObserver {
            override fun onAuthenticated(account: OAuthAccount, authType: AuthType) {
                MainScope().launch { updateSyncPreferenceStatus() }
            }

            override fun onLoggedOut() {
                MainScope().launch { updateSyncPreferenceNeedsLogin() }
            }

            override fun onAuthenticationProblems() {
                MainScope().launch { updateSyncPreferenceNeedsReauth() }
            }
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
     * Shows the current status of the sync preference ("On"/"Off") for the logged in user.
     */
    private fun updateSyncPreferenceStatus() {
        syncPreference.apply {
            val syncEnginesStatus = SyncEnginesStorage(context).getStatus()
            val loginsSyncStatus = syncEnginesStatus.getOrElse(syncEngine) { false }

            summary = context.getString(
                if (loginsSyncStatus) R.string.preferences_passwords_sync_logins_on
                else R.string.preferences_passwords_sync_logins_off
            )

            setOnPreferenceClickListener {
                onSyncStatusClicked()
                true
            }
        }
    }

    /**
     * Display that the user can "Sign in to Sync" when the user is logged off.
     */
    private fun updateSyncPreferenceNeedsLogin() {
        syncPreference.apply {
            summary = context.getString(R.string.preferences_passwords_sync_logins_sign_in)

            setOnPreferenceClickListener {
                onSignInToSyncClicked()
                true
            }
        }
    }

    /**
     * Displays that the user needs to "Reconnect" to fix their account problems with sync.
     */
    private fun updateSyncPreferenceNeedsReauth() {
        syncPreference.apply {
            summary = context.getString(R.string.preferences_passwords_sync_logins_reconnect)

            setOnPreferenceClickListener {
                onReconnectClicked()
                true
            }
        }
    }
}
