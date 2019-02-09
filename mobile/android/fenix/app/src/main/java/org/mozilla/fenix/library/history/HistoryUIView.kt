
/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observable
import io.reactivex.Observer
import io.reactivex.functions.Consumer
import org.mozilla.fenix.R
import org.mozilla.fenix.mvi.UIView

class HistoryUIView(
    container: ViewGroup,
    actionEmitter: Observer<HistoryAction>,
    changesObservable: Observable<HistoryChange>
) :
    UIView<HistoryState, HistoryAction, HistoryChange>(container, actionEmitter, changesObservable) {

    override val view: RecyclerView = LayoutInflater.from(container.context)
        .inflate(R.layout.component_history, container, true)
        .findViewById(R.id.history_list)

    init {
        view.apply {
            adapter = HistoryAdapter(actionEmitter)
            layoutManager = LinearLayoutManager(container.context)
        }
    }

    override fun updateView() = Consumer<HistoryState> {
        (view.adapter as HistoryAdapter).updateData(it.items)
    }
}

private class HistoryAdapter(
    private val actionEmitter: Observer<HistoryAction>
) : RecyclerView.Adapter<HistoryAdapter.HistoryListItemViewHolder>() {
    class HistoryListItemViewHolder(
        view: View,
        private val actionEmitter: Observer<HistoryAction>
    ) : RecyclerView.ViewHolder(view) {

        private var title = view.findViewById<TextView>(R.id.history_title)
        private var url = view.findViewById<TextView>(R.id.history_url)
        private var item: HistoryItem? = null

        init {
            view.setOnClickListener {
                item?.apply {
                    actionEmitter.onNext(HistoryAction.Select(this))
                }
            }
        }

        fun bind(item: HistoryItem) {
            this.item = item

            title.text = item.title
            url.text = item.url
        }

        companion object {
            const val LAYOUT_ID = R.layout.history_list_item
        }
    }

    private var items: List<HistoryItem> = emptyList()

    fun updateData(items: List<HistoryItem>) {
        this.items = items
        notifyDataSetChanged()
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): HistoryListItemViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(viewType, parent, false)

        return HistoryListItemViewHolder(view, actionEmitter)
    }

    override fun getItemViewType(position: Int): Int {
        return HistoryListItemViewHolder.LAYOUT_ID
    }

    override fun getItemCount(): Int = items.count()

    override fun onBindViewHolder(holder: HistoryListItemViewHolder, position: Int) {
        val item = items[position]
        holder.bind(item)
    }
}
