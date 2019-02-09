
/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import android.content.Context
import android.graphics.Color
import android.os.Build
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.RelativeLayout
import android.widget.TextView
import androidx.annotation.RequiresApi
import androidx.constraintlayout.widget.ConstraintLayout
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
            adapter = HistoryAdapter(context)
            layoutManager = LinearLayoutManager(container.context)
        }
    }

    override fun updateView() = Consumer<HistoryState> {
        (view.adapter as HistoryAdapter).updateData(it.items)
    }
}

private class HistoryAdapter(val context: Context) : RecyclerView.Adapter<HistoryAdapter.HistoryListItemViewHolder>() {
    class HistoryListItemViewHolder(val view: View) : RecyclerView.ViewHolder(view) {
        private var title = view.findViewById<TextView>(R.id.history_title)
        private var url = view.findViewById<TextView>(R.id.history_url)

        fun bind(item: HistoryItem) {
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

        return HistoryListItemViewHolder(view)
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
