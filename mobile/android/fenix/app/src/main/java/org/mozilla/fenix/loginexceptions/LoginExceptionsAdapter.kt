/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.loginexceptions

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.feature.logins.exceptions.LoginException
import org.mozilla.fenix.loginexceptions.viewholders.LoginExceptionsDeleteButtonViewHolder
import org.mozilla.fenix.loginexceptions.viewholders.LoginExceptionsHeaderViewHolder
import org.mozilla.fenix.loginexceptions.viewholders.LoginExceptionsListItemViewHolder

/**
 * Adapter for a list of sites that are exempted from saving logins,
 * along with controls to remove the exception.
 */
class LoginExceptionsAdapter(
    private val interactor: LoginExceptionsInteractor
) : ListAdapter<LoginExceptionsAdapter.AdapterItem, RecyclerView.ViewHolder>(DiffCallback) {

    /**
     * Change the list of items that are displayed.
     * Header and footer items are added to the list as well.
     */
    fun updateData(exceptions: List<LoginException>) {
        val adapterItems: List<AdapterItem> = listOf(AdapterItem.Header) +
            exceptions.map { AdapterItem.Item(it) } +
            listOf(AdapterItem.DeleteButton)
        submitList(adapterItems)
    }

    override fun getItemViewType(position: Int) = when (getItem(position)) {
        AdapterItem.DeleteButton -> LoginExceptionsDeleteButtonViewHolder.LAYOUT_ID
        AdapterItem.Header -> LoginExceptionsHeaderViewHolder.LAYOUT_ID
        is AdapterItem.Item -> LoginExceptionsListItemViewHolder.LAYOUT_ID
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(viewType, parent, false)

        return when (viewType) {
            LoginExceptionsDeleteButtonViewHolder.LAYOUT_ID -> LoginExceptionsDeleteButtonViewHolder(
                view,
                interactor
            )
            LoginExceptionsHeaderViewHolder.LAYOUT_ID -> LoginExceptionsHeaderViewHolder(view)
            LoginExceptionsListItemViewHolder.LAYOUT_ID -> LoginExceptionsListItemViewHolder(
                view,
                interactor
            )
            else -> throw IllegalStateException()
        }
    }

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        if (holder is LoginExceptionsListItemViewHolder) {
            val adapterItem = getItem(position) as AdapterItem.Item
            holder.bind(adapterItem.item)
        }
    }

    sealed class AdapterItem {
        object DeleteButton : AdapterItem()
        object Header : AdapterItem()
        data class Item(val item: LoginException) : AdapterItem()
    }

    internal object DiffCallback : DiffUtil.ItemCallback<AdapterItem>() {
        override fun areItemsTheSame(oldItem: AdapterItem, newItem: AdapterItem) =
            when (oldItem) {
                AdapterItem.DeleteButton, AdapterItem.Header -> oldItem === newItem
                is AdapterItem.Item -> newItem is AdapterItem.Item && oldItem.item.id == newItem.item.id
            }

        @Suppress("DiffUtilEquals")
        override fun areContentsTheSame(oldItem: AdapterItem, newItem: AdapterItem) =
            oldItem == newItem
    }
}
