/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.onboarding_finish.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.home.sessioncontrol.OnboardingInteractor

class OnboardingFinishViewHolder(
    view: View,
    private val interactor: OnboardingInteractor
) : RecyclerView.ViewHolder(view) {

    init {
        view.finish_button.setOnClickListener {
            interactor.onStartBrowsingClicked()
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.onboarding_finish
    }
}
