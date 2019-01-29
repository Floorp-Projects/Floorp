/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessions

import android.view.ViewGroup
import androidx.recyclerview.widget.LinearLayoutManager
import io.reactivex.functions.Consumer
import kotlinx.android.synthetic.main.fragment_home.*
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.UIView

class SessionsUIView(container: ViewGroup, bus: ActionBusFactory) : UIView<SessionsState>(container, bus) {

    private var sessionAdapter = SessionsAdapter()

    init {
        session_list.apply {
            layoutManager = LinearLayoutManager(container.context)
            adapter = sessionAdapter
        }
    }

    override fun updateView() = Consumer<SessionsState> {
        
    }

}
