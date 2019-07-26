/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.snackbar.Snackbar
import kotlinx.android.synthetic.main.onboarding_automatic_signin.view.turn_on_sync_button
import kotlinx.android.synthetic.main.onboarding_automatic_signin.view.header_text
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.service.fxa.sharing.ShareableAccount
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.ext.components

class OnboardingAutomaticSignInViewHolder(private val view: View) : RecyclerView.ViewHolder(view) {
    private lateinit var shareableAccount: ShareableAccount

    init {
        view.turn_on_sync_button.setOnClickListener {
            it.turn_on_sync_button.text = it.context.getString(
                R.string.onboarding_firefox_account_signing_in
            )
            it.turn_on_sync_button.isEnabled = false

            CoroutineScope(Dispatchers.Main).launch {
                val result = view.context.components.backgroundServices.accountManager
                    .signInWithShareableAccountAsync(shareableAccount).await()
                if (result) {
                    // Success.
                } else {
                    // Failed to sign-in (either network problem, or bad credentials). Allow to try again.
                    it.turn_on_sync_button.text = it.context.getString(
                        R.string.onboarding_firefox_account_auto_signin_confirm
                    )
                    it.turn_on_sync_button.isEnabled = true
                    FenixSnackbar.make(it, Snackbar.LENGTH_SHORT).setText(
                        it.context.getString(R.string.onboarding_firefox_account_automatic_signin_failed)
                    ).show()
                }
            }
        }
    }

    fun bind(account: ShareableAccount) {
        shareableAccount = account
        view.header_text.text = view.context.getString(
            R.string.onboarding_firefox_account_auto_signin_header_2, account.email
        )
    }

    companion object {
        const val LAYOUT_ID = R.layout.onboarding_automatic_signin
    }
}
