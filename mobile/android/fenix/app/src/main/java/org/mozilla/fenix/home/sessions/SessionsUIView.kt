/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessions

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observable
import io.reactivex.Observer
import io.reactivex.functions.Consumer
import org.mozilla.fenix.R
import org.mozilla.fenix.mvi.UIView

class SessionsUIView(
    container: ViewGroup,
    actionEmitter: Observer<SessionsAction>,
    isPrivate: Boolean,
    changesObservable: Observable<SessionsChange>
) :
    UIView<SessionsState, SessionsAction, SessionsChange>(container, actionEmitter, changesObservable) {

    override val view: RecyclerView = LayoutInflater.from(container.context)
        .inflate(R.layout.component_sessions, container, true)
        .findViewById(R.id.session_list)

    private val sessionsAdapter = SessionsAdapter()

    init {
        view.apply {
            layoutManager = LinearLayoutManager(container.context)
            sessionsAdapter.isPrivate = isPrivate
            sessionsAdapter.context = context
            adapter = sessionsAdapter
        }
    }

    override fun updateView() = Consumer<SessionsState> {
    }
}
