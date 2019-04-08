/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observable
import io.reactivex.Observer
import io.reactivex.functions.Consumer
import org.mozilla.fenix.R
import org.mozilla.fenix.mvi.UIView

// Convert HomeState into a data structure HomeAdapter understands
@SuppressWarnings("ComplexMethod")
private fun SessionControlState.toAdapterList(): List<AdapterItem> {
    val items = mutableListOf<AdapterItem>()

    if (tabs.isNotEmpty()) {
        items.add(AdapterItem.TabHeader)
        tabs.map(AdapterItem::TabItem).forEach { items.add(it) }
        items.add(if (mode == Mode.Private) AdapterItem.DeleteTabs else AdapterItem.ArchiveTabs)
    } else {
        if (mode == Mode.Private) {
            items.add(AdapterItem.PrivateBrowsingDescription)
        }
    }

    if (mode == Mode.Private) { return items }

    if (archivedSessions.isNotEmpty()) {
        items.add(AdapterItem.SessionHeader)
        archivedSessions.map(AdapterItem::SessionItem).forEach { items.add(it) }
    } else {
        items.add(AdapterItem.SessionPlaceholder)
    }

    return items
}

class SessionControlUIView(
    container: ViewGroup,
    actionEmitter: Observer<SessionControlAction>,
    changesObservable: Observable<SessionControlChange>
) :
    UIView<SessionControlState, SessionControlAction, SessionControlChange>(
        container,
        actionEmitter,
        changesObservable
    ) {

    override val view: RecyclerView = LayoutInflater.from(container.context)
        .inflate(R.layout.component_session_control, container, true)
        .findViewById(R.id.home_component)

    private val sessionControlAdapter = SessionControlAdapter(actionEmitter)

    init {
        view.apply {
            adapter = sessionControlAdapter
            layoutManager = LinearLayoutManager(container.context)
        }
    }

    override fun updateView() = Consumer<SessionControlState> {
        sessionControlAdapter.reloadData(it.toAdapterList())

        // There is a current bug in the combination of MotionLayout~alhpa4 and RecyclerView where it doesn't think
        // it has to redraw itself. For some reason calling scrollBy forces this to happen every time
        // https://stackoverflow.com/a/42549611
        view.scrollBy(0, 0)
    }
}
