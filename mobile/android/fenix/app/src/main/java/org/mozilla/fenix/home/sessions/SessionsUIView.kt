/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessions

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.functions.Consumer
import kotlinx.android.synthetic.main.component_sessions.*
import kotlinx.android.synthetic.main.fragment_home.*
import org.mozilla.fenix.R
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.UIView

class SessionsUIView(container: ViewGroup, bus: ActionBusFactory) : UIView<SessionsState>(container, bus) {

    val view: ConstraintLayout = LayoutInflater.from(container.context)
        .inflate(R.layout.component_sessions, container, true) as ConstraintLayout

    private var sessionAdapter = SessionsAdapter()

    init {
        session_list.apply {
            layoutManager = LinearLayoutManager(view.context)
            adapter = sessionAdapter
            setHasFixedSize(true)
        }
    }

    override fun updateView() = Consumer<SessionsState> {
        
    }

}
