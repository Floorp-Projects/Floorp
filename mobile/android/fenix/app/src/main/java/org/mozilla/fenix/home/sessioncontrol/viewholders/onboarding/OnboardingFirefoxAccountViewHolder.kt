/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.View
import androidx.appcompat.content.res.AppCompatResources
import androidx.navigation.Navigation
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.onboarding_firefox_account.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.home.HomeFragmentDirections

class OnboardingFirefoxAccountViewHolder(private val view: View) : RecyclerView.ViewHolder(view) {
    private val avatarAnonymousDrawable by lazy {
        AppCompatResources.getDrawable(view.context, R.drawable.ic_onboarding_avatar_anonymous)
    }
    private val firefoxAccountsDrawable by lazy {
        AppCompatResources.getDrawable(view.context, R.drawable.ic_onboarding_firefox_accounts)
    }

    init {
        view.turn_on_sync_button.setOnClickListener {
            val directions = HomeFragmentDirections.actionHomeFragmentToTurnOnSyncFragment()
            Navigation.findNavController(view).navigate(directions)
        }
    }

    fun bind(autoSignedIn: Boolean) {
        updateHeaderText(autoSignedIn)
        updateButtonVisibility(autoSignedIn)
    }

    private fun updateButtonVisibility(autoSignedIn: Boolean) {
        view.turn_on_sync_button.visibility = if (autoSignedIn) View.GONE else View.VISIBLE
        view.stay_signed_in_button.visibility = if (autoSignedIn) View.VISIBLE else View.GONE
        view.sign_out_button.visibility = if (autoSignedIn) View.VISIBLE else View.GONE
    }

    private fun updateHeaderText(autoSignedIn: Boolean) {
        val icon = if (autoSignedIn) avatarAnonymousDrawable else firefoxAccountsDrawable
        view.header_text.setCompoundDrawablesRelativeWithIntrinsicBounds(icon, null, null, null)

        val appName = view.context.getString(R.string.app_name)
        view.header_text.text =
            if (autoSignedIn) view.context.getString(R.string.onboarding_firefox_account_auto_signin_header)
            else view.context.getString(R.string.onboarding_firefox_account_header, appName)
    }

    companion object {
        const val LAYOUT_ID = R.layout.onboarding_firefox_account
    }
}
