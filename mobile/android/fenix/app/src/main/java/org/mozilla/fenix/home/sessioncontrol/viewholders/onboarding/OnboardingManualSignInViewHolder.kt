/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.View
import androidx.navigation.Navigation
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.onboarding_manual_signin.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.addUnderline
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.home.HomeFragmentDirections
import org.mozilla.fenix.onboarding.OnboardingController
import org.mozilla.fenix.onboarding.OnboardingInteractor

class OnboardingManualSignInViewHolder(view: View) : RecyclerView.ViewHolder(view) {

    private val headerText = view.header_text

    init {
        val interactor = OnboardingInteractor(OnboardingController(itemView.context))

        view.fxa_sign_in_button.setOnClickListener {
            it.context.components.analytics.metrics.track(Event.OnboardingManualSignIn)

            val directions = HomeFragmentDirections.actionGlobalTurnOnSync()
            Navigation.findNavController(view).navigate(directions)
        }

        view.learn_more.addUnderline()
        view.learn_more.setOnClickListener {
            interactor.onLearnMoreClicked()
        }
    }

    fun bind() {
        val context = itemView.context
        headerText.text = context.getString(R.string.onboarding_firefox_account_header)
    }

    companion object {
        const val LAYOUT_ID = R.layout.onboarding_manual_signin
    }
}
