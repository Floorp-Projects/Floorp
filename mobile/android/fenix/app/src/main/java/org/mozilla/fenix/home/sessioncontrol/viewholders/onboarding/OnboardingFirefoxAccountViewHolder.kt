/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.View
import androidx.navigation.Navigation
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.onboarding_firefox_account.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.home.HomeFragmentDirections

class OnboardingFirefoxAccountViewHolder(view: View) : RecyclerView.ViewHolder(view) {

    init {
        val appName = view.context.getString(R.string.app_name)
        view.header_text.text = view.context.getString(R.string.onboarding_firefox_account_header, appName)

        view.turn_on_sync_button.setOnClickListener {
            val directions = HomeFragmentDirections.actionHomeFragmentToTurnOnSyncFragment()
            Navigation.findNavController(view).navigate(directions)
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.onboarding_firefox_account
    }
}
