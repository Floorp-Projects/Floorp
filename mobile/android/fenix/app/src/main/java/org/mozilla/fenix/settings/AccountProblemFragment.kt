/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.NavHostFragment
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import kotlinx.coroutines.launch
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.Profile
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents

class AccountProblemFragment : PreferenceFragmentCompat(), AccountObserver {

    override fun onResume() {
        super.onResume()
        (activity as AppCompatActivity).title = getString(R.string.sync_reconnect)
        (activity as AppCompatActivity).supportActionBar?.show()

        // We may have fixed our auth problem, in which case close this fragment.
        if (requireComponents.backgroundServices.accountManager.authenticatedAccount() != null &&
            !requireComponents.backgroundServices.accountManager.accountNeedsReauth()
        ) {
            NavHostFragment.findNavController(this).popBackStack()
            return
        }

        requireComponents.backgroundServices.accountManager.register(this, owner = this)
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
            nav(
                R.id.accountProblemFragment,
                AccountProblemFragmentDirections.actionAccountProblemFragmentToSignOutFragment()
            )
            true
        }
    }

    // We're told our auth problems have been fixed; close this fragment.
    override fun onAuthenticated(account: OAuthAccount) {
        lifecycleScope.launch {
            NavHostFragment.findNavController(this@AccountProblemFragment).popBackStack()
        }
    }

    override fun onAuthenticationProblems() {}

    // We're told there are no more auth problems since there is no more account; close this fragment.
    override fun onLoggedOut() {
        lifecycleScope.launch {
            NavHostFragment.findNavController(this@AccountProblemFragment).popBackStack()
        }
    }

    override fun onProfileUpdated(profile: Profile) {}
}
