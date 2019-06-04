/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.navigation.Navigation
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.ext.requireComponents

class AccountProblemFragment : PreferenceFragmentCompat() {

    override fun onResume() {
        super.onResume()
        (activity as AppCompatActivity).title = getString(R.string.sync_reconnect)
        (activity as AppCompatActivity).supportActionBar?.show()
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.sync_problem, rootKey)

        val preferenceSignIn =
            findPreference<Preference>(context!!.getPreferenceKey(R.string.pref_key_sync_sign_in))
        val preferenceSignOut =
            findPreference<Preference>(context!!.getPreferenceKey(R.string.pref_key_sign_out))
        preferenceSignIn?.onPreferenceClickListener = getClickListenerForSignIn()
        preferenceSignOut?.onPreferenceClickListener = getClickListenerForSignOut()
    }

    private fun getClickListenerForSignIn(): Preference.OnPreferenceClickListener {
        return Preference.OnPreferenceClickListener {
            requireComponents.services.accountsAuthFeature.beginAuthentication(requireContext())
            // TODO The sign-in web content populates session history,
            // so pressing "back" after signing in won't take us back into the settings screen, but rather up the
            // session history stack.
            // We could auto-close this tab once we get to the end of the authentication process?
            // Via an interceptor, perhaps.
            view?.let {
                (activity as HomeActivity).openToBrowser(BrowserDirection.FromAccountProblem)
            }
            true
        }
    }

    private fun getClickListenerForSignOut(): Preference.OnPreferenceClickListener {
        return Preference.OnPreferenceClickListener {
            CoroutineScope(Dispatchers.Main).launch {
                requireComponents.backgroundServices.accountManager.logoutAsync().await()
                Navigation.findNavController(view!!).popBackStack()
            }
            true
        }
    }
}
