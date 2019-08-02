/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.View
import androidx.appcompat.content.res.AppCompatResources
import androidx.navigation.Navigation
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.onboarding_manual_signin.view.*
import mozilla.components.support.ktx.android.view.putCompoundDrawablesRelativeWithIntrinsicBounds
import org.mozilla.fenix.R
import org.mozilla.fenix.home.HomeFragmentDirections

class OnboardingManualSignInViewHolder(private val view: View) : RecyclerView.ViewHolder(view) {
    init {
        view.turn_on_sync_button.setOnClickListener {
            val directions = HomeFragmentDirections.actionHomeFragmentToTurnOnSyncFragment()
            Navigation.findNavController(view).navigate(directions)
        }
    }

    fun bind() {
        val appName = view.context.getString(R.string.app_name)
        view.header_text.text = view.context.getString(R.string.onboarding_firefox_account_header, appName)
        val icon = AppCompatResources.getDrawable(view.context, R.drawable.ic_onboarding_firefox_accounts)
        view.header_text.putCompoundDrawablesRelativeWithIntrinsicBounds(start = icon)
    }

    companion object {
        const val LAYOUT_ID = R.layout.onboarding_manual_signin
    }
}
