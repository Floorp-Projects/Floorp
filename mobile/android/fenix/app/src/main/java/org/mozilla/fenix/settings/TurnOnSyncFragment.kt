/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.core.text.HtmlCompat
import androidx.fragment.app.Fragment
import androidx.navigation.Navigation
import androidx.navigation.fragment.NavHostFragment.findNavController
import kotlinx.android.synthetic.main.fragment_turn_on_sync.view.*
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.OAuthAccount
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.requireComponents

@SuppressWarnings("TooManyFunctions")
class TurnOnSyncFragment : Fragment(), AccountObserver {
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

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        val view = inflater.inflate(R.layout.fragment_turn_on_sync, container, false)
        view.signInScanButton.setOnClickListener(getClickListenerForPairing())
        view.signInEmailButton.setOnClickListener(getClickListenerForSignIn())
        view.signInInstructions.text = HtmlCompat.fromHtml(
            getString(R.string.sign_in_instructions),
            HtmlCompat.FROM_HTML_MODE_LEGACY
        )
        return view
    }

    private fun getClickListenerForSignIn(): View.OnClickListener {
        return View.OnClickListener {
            requireComponents.services.accountsAuthFeature.beginAuthentication(requireContext())
            // TODO The sign-in web content populates session history,
            // so pressing "back" after signing in won't take us back into the settings screen, but rather up the
            // session history stack.
            // We could auto-close this tab once we get to the end of the authentication process?
            // Via an interceptor, perhaps.
        }
    }

    private fun getClickListenerForPairing(): View.OnClickListener {
        return View.OnClickListener {
            val directions = TurnOnSyncFragmentDirections.actionTurnOnSyncFragmentToPairFragment()
            Navigation.findNavController(view!!).navigate(directions)
            requireComponents.analytics.metrics.track(Event.SyncAuthScanPairing)
        }
    }

    override fun onAuthenticated(account: OAuthAccount, newAccount: Boolean) {
        FenixSnackbar.make(view!!, FenixSnackbar.LENGTH_SHORT)
            .setText(requireContext().getString(R.string.sync_syncing_in_progress))
            .show()
    }
}
