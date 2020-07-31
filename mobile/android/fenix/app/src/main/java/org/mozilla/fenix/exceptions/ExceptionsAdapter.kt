/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.annotation.LayoutRes
import androidx.annotation.StringRes
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.exceptions.viewholders.ExceptionsDeleteButtonViewHolder
import org.mozilla.fenix.exceptions.viewholders.ExceptionsHeaderViewHolder
import org.mozilla.fenix.exceptions.viewholders.ExceptionsListItemViewHolder

/**
 * Adapter for a list of sites that are exempted from saving logins or tracking protection,
 * along with controls to remove the exception.
 */
abstract class ExceptionsAdapter<T : Any>(
    private val interactor: ExceptionsInteractor<T>,
    diffCallback: DiffUtil.ItemCallback<AdapterItem>
) : ListAdapter<ExceptionsAdapter.AdapterItem, RecyclerView.ViewHolder>(diffCallback) {

    /**
     * Change the list of items that are displayed.
     * Header and footer items are added to the list as well.
     */
    fun updateData(exceptions: List<T>) {
        val adapterItems: List<AdapterItem> = listOf(AdapterItem.Header) +
            exceptions.map { wrapAdapterItem(it) } +
            listOf(AdapterItem.DeleteButton)
        submitList(adapterItems)
    }

    /**
     * Layout to use for the delete button.
     */
    @get:LayoutRes
    abstract val deleteButtonLayoutId: Int

    /**
     * String to use for the exceptions list header.
     */
    @get:StringRes
    abstract val headerDescriptionResource: Int

    /**
     * Converts an item from [updateData] into an adapter item.
     */
    abstract fun wrapAdapterItem(item: T): AdapterItem.Item<T>

    final override fun getItemViewType(position: Int) = when (getItem(position)) {
        AdapterItem.DeleteButton -> deleteButtonLayoutId
        AdapterItem.Header -> ExceptionsHeaderViewHolder.LAYOUT_ID
        is AdapterItem.Item<*> -> ExceptionsListItemViewHolder.LAYOUT_ID
    }

    final override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(viewType, parent, false)

        return when (viewType) {
            deleteButtonLayoutId ->
                ExceptionsDeleteButtonViewHolder(view, interactor)
            ExceptionsHeaderViewHolder.LAYOUT_ID ->
                ExceptionsHeaderViewHolder(view, headerDescriptionResource)
            ExceptionsListItemViewHolder.LAYOUT_ID ->
                ExceptionsListItemViewHolder(view, interactor)
            else -> throw IllegalStateException()
        }
    }

    @Suppress("Unchecked_Cast")
    final override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        if (holder is ExceptionsListItemViewHolder<*>) {
            holder as ExceptionsListItemViewHolder<T>
            val adapterItem = getItem(position) as AdapterItem.Item<T>
            holder.bind(adapterItem.item, adapterItem.url)
        }
    }

    /**
     * Internal items for [ExceptionsAdapter]
     */
    sealed class AdapterItem {
        object DeleteButton : AdapterItem()
        object Header : AdapterItem()

        /**
         * Represents an item to display in [ExceptionsAdapter].
         * [T] should refer to the same value as in the [ExceptionsAdapter] and [ExceptionsInteractor].
         */
        abstract class Item<T> : AdapterItem() {
            abstract val item: T
            abstract val url: String
        }
    }
}
