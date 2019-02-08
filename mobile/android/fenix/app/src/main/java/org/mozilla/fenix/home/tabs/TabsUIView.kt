/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.tabs

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.recyclerview.widget.DefaultItemAnimator
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observable
import io.reactivex.Observer
import io.reactivex.functions.Consumer
import org.mozilla.fenix.R
import org.mozilla.fenix.mvi.UIView

class TabsUIView(
    container: ViewGroup,
    actionEmitter: Observer<TabsAction>,
    changesObservable: Observable<TabsChange>
) :
    UIView<TabsState, TabsAction, TabsChange>(container, actionEmitter, changesObservable) {

    private val header: ConstraintLayout = LayoutInflater.from(container.context)
        .inflate(R.layout.tab_list_header, container, true)
        .findViewById(R.id.tabs_header)

    override val view: RecyclerView = LayoutInflater.from(container.context)
        .inflate(R.layout.component_tabs, container, true)
        .findViewById(R.id.tabs_list)

    private val tabsAdapter = TabsAdapter(actionEmitter)

    init {
        view.apply {
            layoutManager = LinearLayoutManager(container.context)
            adapter = tabsAdapter
            itemAnimator = DefaultItemAnimator()
        }
    }

    override fun updateView() = Consumer<TabsState> {
        tabsAdapter.sessions = it.sessions
        header.visibility = if (it.sessions.isEmpty()) View.GONE else View.VISIBLE
    }
}
