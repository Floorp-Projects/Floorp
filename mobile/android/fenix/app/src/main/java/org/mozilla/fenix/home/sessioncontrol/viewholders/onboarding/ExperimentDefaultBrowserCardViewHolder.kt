/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.experiment_default_browser.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.increaseTapArea
import org.mozilla.fenix.home.sessioncontrol.SessionControlInteractor

class ExperimentDefaultBrowserCardViewHolder(
    view: View,
    private val interactor: SessionControlInteractor
) : RecyclerView.ViewHolder(view) {

    init {
        view.set_default_browser.setOnClickListener {
            interactor.onSetDefaultBrowserClicked()
        }

        view.close.apply {
            increaseTapArea(CLOSE_BUTTON_EXTRA_DPS)
            setOnClickListener {
                interactor.onCloseExperimentCardClicked()
            }
        }
    }

    companion object {
        internal const val LAYOUT_ID = R.layout.experiment_default_browser
        private const val CLOSE_BUTTON_EXTRA_DPS = 38
    }
}
