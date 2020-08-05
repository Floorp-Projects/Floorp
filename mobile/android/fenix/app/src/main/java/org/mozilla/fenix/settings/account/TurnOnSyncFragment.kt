/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.account

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.text.HtmlCompat
import androidx.fragment.app.Fragment
import androidx.navigation.findNavController
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import kotlinx.android.synthetic.main.fragment_turn_on_sync.view.*
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.support.ktx.android.content.hasCamera
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.showToolbar

class TurnOnSyncFragment : Fragment(), AccountObserver {

    private val args by navArgs<TurnOnSyncFragmentArgs>()
    private var shouldLoginJustWithEmail = false
    private var pairWithEmailStarted = false

    private val signInClickListener = View.OnClickListener {
        navigateToPairWithEmail()
    }

    private val paringClickListener = View.OnClickListener {
        val directions = TurnOnSyncFragmentDirections.actionTurnOnSyncFragmentToPairFragment()
        requireView().findNavController().navigate(directions)
        requireComponents.analytics.metrics.track(Event.SyncAuthScanPairing)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        requireComponents.analytics.metrics.track(Event.SyncAuthOpened)

        // App can be installed on devices with no camera modules. Like Android TV boxes.
        // Let's skip presenting the option to sign in by scanning a qr code in this case
        // and default to login with email and password.
        shouldLoginJustWithEmail = !requireContext().hasCamera()
        if (shouldLoginJustWithEmail) {
            navigateToPairWithEmail()
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        requireComponents.analytics.metrics.track(Event.SyncAuthClosed)
    }

    override fun onResume() {
        super.onResume()
        if (pairWithEmailStarted ||
            requireComponents.backgroundServices.accountManager.authenticatedAccount() != null) {

            findNavController().popBackStack()
            return
        }

        if (shouldLoginJustWithEmail) {
            // Next time onResume is called, after returning from pairing with email this Fragment will be popped.
            pairWithEmailStarted = true
        } else {
            requireComponents.backgroundServices.accountManager.register(this, owner = this)
            showToolbar(getString(R.string.preferences_sync))
        }
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        if (shouldLoginJustWithEmail) {
            // Headless fragment. Don't need UI if we're taking the user to another screen.
            return null
        }

        val view = inflater.inflate(R.layout.fragment_turn_on_sync, container, false)
        view.signInScanButton.setOnClickListener(paringClickListener)
        view.signInEmailButton.setOnClickListener(signInClickListener)
        view.signInInstructions.text = HtmlCompat.fromHtml(
            getString(R.string.sign_in_instructions),
            HtmlCompat.FROM_HTML_MODE_LEGACY
        )
        return view
    }

    override fun onAuthenticated(account: OAuthAccount, authType: AuthType) {
        val snackbarText = requireContext().getString(R.string.sync_syncing_in_progress)
        val snackbarLength = FenixSnackbar.LENGTH_SHORT

        // Since the snackbar can be presented in BrowserFragment or in SettingsFragment we must
        // base our display method on the padSnackbar argument
        if (args.padSnackbar) {
            FenixSnackbar.make(
                view = requireView(),
                duration = snackbarLength,
                isDisplayedWithBrowserToolbar = true
            )
                .setText(snackbarText)
                .show()
        } else {
            FenixSnackbar.make(
                view = requireView(),
                duration = snackbarLength,
                isDisplayedWithBrowserToolbar = false
            )
                .setText(snackbarText)
                .show()
        }
    }

    private fun navigateToPairWithEmail() {
        requireComponents.services.accountsAuthFeature.beginAuthentication(requireContext())
        requireComponents.analytics.metrics.track(Event.SyncAuthUseEmail)
        // TODO The sign-in web content populates session history,
        // so pressing "back" after signing in won't take us back into the settings screen, but rather up the
        // session history stack.
        // We could auto-close this tab once we get to the end of the authentication process?
        // Via an interceptor, perhaps.
    }
}
