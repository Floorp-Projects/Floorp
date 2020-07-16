/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.onboarding_whats_new.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.addUnderline
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.home.sessioncontrol.OnboardingInteractor

class OnboardingWhatsNewViewHolder(
    view: View,
    private val interactor: OnboardingInteractor
) : RecyclerView.ViewHolder(view) {

    init {
        view.header_text.setOnboardingIcon(R.drawable.ic_whats_new)

        val appName = view.context.getString(R.string.app_name)
        view.description_text.text = view.context.getString(R.string.onboarding_whats_new_description, appName)

        view.get_answers.addUnderline()
        view.get_answers.setOnClickListener {
            interactor.onWhatsNewGetAnswersClicked()
            view.context.components.analytics.metrics.track(Event.OnboardingWhatsNew)
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.onboarding_whats_new
    }
}
