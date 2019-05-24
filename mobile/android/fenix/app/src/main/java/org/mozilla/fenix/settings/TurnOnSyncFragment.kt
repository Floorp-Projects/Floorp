/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.navigation.Navigation
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.ext.requireComponents

class TurnOnSyncFragment : PreferenceFragmentCompat() {

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
        (activity as AppCompatActivity).title = getString(R.string.preferences_sync)
        (activity as AppCompatActivity).supportActionBar?.show()
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.turn_on_sync_preferences, rootKey)

        val preferenceSignIn =
            findPreference<Preference>(context!!.getPreferenceKey(R.string.pref_key_sync_sign_in))
        val preferenceNewAccount =
            findPreference<Preference>(context!!.getPreferenceKey(R.string.pref_key_sync_create_account))
        val preferencePairSignIn =
            findPreference<Preference>(context!!.getPreferenceKey(R.string.pref_key_sync_pair))
        preferenceSignIn?.onPreferenceClickListener = getClickListenerForSignIn()
        preferenceNewAccount?.onPreferenceClickListener = getClickListenerForCreateAccount()
        preferencePairSignIn?.onPreferenceClickListener = getClickListenerForPairing()
    }

    private fun getClickListenerForSignIn(): Preference.OnPreferenceClickListener {
        return Preference.OnPreferenceClickListener {
            requireComponents.services.accountsAuthFeature.beginAuthentication()
            // TODO The sign-in web content populates session history,
            // so pressing "back" after signing in won't take us back into the settings screen, but rather up the
            // session history stack.
            // We could auto-close this tab once we get to the end of the authentication process?
            // Via an interceptor, perhaps.
            requireComponents.analytics.metrics.track(Event.SyncAuthSignIn)
            view?.let {
                (activity as HomeActivity).openToBrowser(BrowserDirection.FromTurnOnSync)
            }
            true
        }
    }

    private fun getClickListenerForCreateAccount(): Preference.OnPreferenceClickListener {
        // Currently the same as sign in, as FxA handles this, however we want to emit a different telemetry event
        return Preference.OnPreferenceClickListener {
            requireComponents.services.accountsAuthFeature.beginAuthentication()
            requireComponents.analytics.metrics.track(Event.SyncAuthCreateAccount)
            view?.let {
                (activity as HomeActivity).openToBrowser(BrowserDirection.FromTurnOnSync)
            }
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
}
