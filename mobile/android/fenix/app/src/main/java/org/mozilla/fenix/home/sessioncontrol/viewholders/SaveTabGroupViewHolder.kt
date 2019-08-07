/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observer
import kotlinx.android.synthetic.main.save_tab_group_button.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.home.sessioncontrol.SessionControlAction
import org.mozilla.fenix.home.sessioncontrol.TabAction
import org.mozilla.fenix.home.sessioncontrol.onNext

class SaveTabGroupViewHolder(
    view: View,
    private val actionEmitter: Observer<SessionControlAction>
) : RecyclerView.ViewHolder(view) {

    init {
        view.save_tab_group_button.setOnClickListener {
            view.context.components.analytics.metrics
                .track(Event.CollectionSaveButtonPressed(TELEMETRY_HOME_IDENTIFIER))

            actionEmitter.onNext(TabAction.SaveTabGroup(selectedTabSessionId = null))
        }
    }

    companion object {
        const val TELEMETRY_HOME_IDENTIFIER = "home"
        const val LAYOUT_ID = R.layout.save_tab_group_button
    }
}
