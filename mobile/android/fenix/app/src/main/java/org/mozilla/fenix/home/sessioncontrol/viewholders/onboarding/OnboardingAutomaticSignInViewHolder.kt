/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.View
import android.widget.Button
import androidx.annotation.VisibleForTesting
import androidx.appcompat.content.res.AppCompatResources.getDrawable
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.snackbar.Snackbar
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import mozilla.components.service.fxa.manager.MigrationResult
import mozilla.components.service.fxa.sharing.ShareableAccount
import mozilla.components.support.ktx.android.view.putCompoundDrawablesRelativeWithIntrinsicBounds
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.databinding.OnboardingAutomaticSigninBinding
import org.mozilla.fenix.ext.components

class OnboardingAutomaticSignInViewHolder(
    view: View,
    private val scope: CoroutineScope = MainScope()
) : RecyclerView.ViewHolder(view) {

    private lateinit var shareableAccount: ShareableAccount
    private val binding = OnboardingAutomaticSigninBinding.bind(view)
    private val headerText = binding.headerText

    init {
        binding.fxaSignInButton.setOnClickListener {
            scope.launch {
                onClick(binding.fxaSignInButton)
            }
        }
    }

    fun bind(account: ShareableAccount) {
        shareableAccount = account
        headerText.text = itemView.context.getString(
            R.string.onboarding_firefox_account_auto_signin_header_3, account.email
        )
        val icon = getDrawable(itemView.context, R.drawable.ic_onboarding_avatar_anonymous)
        headerText.putCompoundDrawablesRelativeWithIntrinsicBounds(start = icon)
    }

    @VisibleForTesting
    internal suspend fun onClick(button: Button) {
        val context = button.context
        context.components.analytics.metrics.track(Event.OnboardingAutoSignIn)

        button.text = context.getString(R.string.onboarding_firefox_account_signing_in)
        button.isEnabled = false

        val accountManager = context.components.backgroundServices.accountManager
        when (accountManager.migrateFromAccount(shareableAccount)) {
            MigrationResult.WillRetry,
            MigrationResult.Success -> {
                // We consider both of these as a 'success'.
            }
            MigrationResult.Failure -> {
                // Failed to sign-in (e.g. bad credentials). Allow to try again.
                button.text = context.getString(R.string.onboarding_firefox_account_auto_signin_confirm)
                button.isEnabled = true
                FenixSnackbar.make(
                    view = button,
                    duration = Snackbar.LENGTH_SHORT,
                    isDisplayedWithBrowserToolbar = false
                ).setText(
                    context.getString(R.string.onboarding_firefox_account_automatic_signin_failed)
                ).show()
            }
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.onboarding_automatic_signin
    }
}
