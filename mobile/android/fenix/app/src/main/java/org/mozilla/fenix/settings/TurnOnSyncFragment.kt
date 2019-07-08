/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.navigation.Navigation
import androidx.navigation.fragment.NavHostFragment.findNavController
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.Profile
import mozilla.components.support.ktx.android.content.hasCamera
import org.mozilla.fenix.Experiments
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.isInExperiment

@SuppressWarnings("TooManyFunctions")
class TurnOnSyncFragment : PreferenceFragmentCompat(), AccountObserver {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        requireComponents.analytics.metrics.track(Event.SyncAuthOpened)
    }

    override fun onDestroy() {
        super.onDestroy()
        requireComponents.analytics.metrics.track(Event.SyncAuthClosed)
    }

    override fun onResume() {
        super.onResume()
        if (requireComponents.backgroundServices.accountManager.authenticatedAccount() != null) {
            findNavController(this).popBackStack()
            return
        }

        requireComponents.backgroundServices.accountManager.register(this, owner = this)
        (activity as AppCompatActivity).title = getString(R.string.preferences_sync)
        (activity as AppCompatActivity).supportActionBar?.show()
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.turn_on_sync_preferences, rootKey)

        val preferenceSignIn =
            findPreference<Preference>(context!!.getPreferenceKey(R.string.pref_key_sync_sign_in))
        val preferencePairSignIn =
            findPreference<Preference>(context!!.getPreferenceKey(R.string.pref_key_sync_pair))
        preferenceSignIn?.onPreferenceClickListener = getClickListenerForSignIn()
        preferencePairSignIn?.onPreferenceClickListener = getClickListenerForPairing()
        preferencePairSignIn?.isVisible = context?.hasCamera() ?: true

        // if FxA pairing has been turned off on the server
        if (context?.isInExperiment(Experiments.asFeatureFxAPairingDisabled)!!) {
            preferencePairSignIn?.isVisible = false
        }
    }

    private fun getClickListenerForSignIn(): Preference.OnPreferenceClickListener {
        return Preference.OnPreferenceClickListener {
            requireComponents.services.accountsAuthFeature.beginAuthentication(requireContext())
            // TODO The sign-in web content populates session history,
            // so pressing "back" after signing in won't take us back into the settings screen, but rather up the
            // session history stack.
            // We could auto-close this tab once we get to the end of the authentication process?
            // Via an interceptor, perhaps.
            requireComponents.analytics.metrics.track(Event.SyncAuthSignIn)
            true
        }
    }

    private fun getClickListenerForPairing(): Preference.OnPreferenceClickListener {
        return Preference.OnPreferenceClickListener {
            val directions = TurnOnSyncFragmentDirections.actionTurnOnSyncFragmentToPairInstructionsFragment()
            Navigation.findNavController(view!!).navigate(directions)
            requireComponents.analytics.metrics.track(Event.SyncAuthScanPairing)

            true
        }
    }

    override fun onAuthenticated(account: OAuthAccount) {
        FenixSnackbar.make(view!!, FenixSnackbar.LENGTH_SHORT)
            .setText(requireContext().getString(R.string.sync_syncing_in_progress))
            .show()
    }

    override fun onAuthenticationProblems() {}
    override fun onError(error: Exception) {}
    override fun onLoggedOut() {}
    override fun onProfileUpdated(profile: Profile) {} }
