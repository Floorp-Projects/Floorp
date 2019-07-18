/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.exceptions.viewholders.ExceptionsDeleteButtonViewHolder
import org.mozilla.fenix.exceptions.viewholders.ExceptionsHeaderViewHolder
import org.mozilla.fenix.exceptions.viewholders.ExceptionsListItemViewHolder
import org.mozilla.fenix.utils.AdapterWithJob

private sealed class AdapterItem {
    object DeleteButton : AdapterItem()
    object Header : AdapterItem()
    data class Item(val item: ExceptionsItem) : AdapterItem()
}

private class ExceptionsList(val exceptions: List<ExceptionsItem>) {
    val items: List<AdapterItem>

    init {
        val items = mutableListOf<AdapterItem>()
        items.add(AdapterItem.Header)
        for (exception in exceptions) {
            items.add(AdapterItem.Item(exception))
        }
        items.add(AdapterItem.DeleteButton)
        this.items = items
    }
}

class ExceptionsAdapter(
    private val interactor: ExceptionsInteractor
) : AdapterWithJob<RecyclerView.ViewHolder>() {
    private var exceptionsList: ExceptionsList = ExceptionsList(emptyList())

    fun updateData(items: List<ExceptionsItem>) {
        this.exceptionsList = ExceptionsList(items)
        notifyDataSetChanged()
    }

    override fun getItemCount(): Int = exceptionsList.items.size

    override fun getItemViewType(position: Int): Int {
        return when (exceptionsList.items[position]) {
            is AdapterItem.DeleteButton -> ExceptionsDeleteButtonViewHolder.LAYOUT_ID
            is AdapterItem.Header -> ExceptionsHeaderViewHolder.LAYOUT_ID
            is AdapterItem.Item -> ExceptionsListItemViewHolder.LAYOUT_ID
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(viewType, parent, false)

        return when (viewType) {
            ExceptionsDeleteButtonViewHolder.LAYOUT_ID -> ExceptionsDeleteButtonViewHolder(
                view,
                interactor
            )
            ExceptionsHeaderViewHolder.LAYOUT_ID -> ExceptionsHeaderViewHolder(view)
            ExceptionsListItemViewHolder.LAYOUT_ID -> ExceptionsListItemViewHolder(
                view,
                interactor,
                adapterJob
            )
            else -> throw IllegalStateException()
        }
    }

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        when (holder) {
            is ExceptionsListItemViewHolder -> (exceptionsList.items[position] as AdapterItem.Item).also {
                holder.bind(it.item)
            }
        }
    }
}
