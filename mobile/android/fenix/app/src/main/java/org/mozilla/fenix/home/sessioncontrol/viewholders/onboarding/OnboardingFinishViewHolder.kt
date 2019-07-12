/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observer
import kotlinx.android.synthetic.main.onboarding_finish.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.home.sessioncontrol.OnboardingAction
import org.mozilla.fenix.home.sessioncontrol.SessionControlAction
import org.mozilla.fenix.home.sessioncontrol.onNext

class OnboardingFinishViewHolder(
    view: View,
    private val actionEmitter: Observer<SessionControlAction>
) : RecyclerView.ViewHolder(view) {

    init {
        view.finish_button.setOnClickListener {
            actionEmitter.onNext(OnboardingAction.Finish)
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.onboarding_finish
    }
}
