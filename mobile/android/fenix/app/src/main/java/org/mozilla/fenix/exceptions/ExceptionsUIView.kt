/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observable
import io.reactivex.Observer
import io.reactivex.functions.Consumer
import kotlinx.android.synthetic.main.component_exceptions.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.mvi.UIView

class ExceptionsUIView(
    container: ViewGroup,
    actionEmitter: Observer<ExceptionsAction>,
    changesObservable: Observable<ExceptionsChange>
) :
    UIView<ExceptionsState, ExceptionsAction, ExceptionsChange>(container, actionEmitter, changesObservable) {

    override val view: RecyclerView = LayoutInflater.from(container.context)
        .inflate(R.layout.component_exceptions, container, true)
        .findViewById(R.id.exceptions_list)

    init {
        view.exceptions_list.apply {
            adapter = ExceptionsAdapter(actionEmitter)
            layoutManager = LinearLayoutManager(container.context)
        }
    }

    override fun updateView() = Consumer<ExceptionsState> {
        (view.exceptions_list.adapter as ExceptionsAdapter).updateData(it.items)
    }
}
