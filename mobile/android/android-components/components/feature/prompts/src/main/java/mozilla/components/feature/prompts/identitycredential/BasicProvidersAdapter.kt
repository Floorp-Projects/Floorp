/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.identitycredential

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.concept.identitycredential.Provider
import mozilla.components.feature.prompts.R

private object ProviderItemDiffCallback : DiffUtil.ItemCallback<Provider>() {
    override fun areItemsTheSame(oldItem: Provider, newItem: Provider) =
        oldItem.id == newItem.id

    override fun areContentsTheSame(oldItem: Provider, newItem: Provider) =
        oldItem == newItem
}

/**
 * RecyclerView adapter for displaying [Provider] items.
 */
internal class BasicProviderAdapter(
    private val onlProviderSelected: (Provider) -> Unit,
) : ListAdapter<Provider, ProviderViewHolder>(ProviderItemDiffCallback) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ProviderViewHolder {
        val view = LayoutInflater
            .from(parent.context)
            .inflate(R.layout.mozac_feature_prompts_identity_crendential_provider_item, parent, false)
        return ProviderViewHolder(view, onlProviderSelected)
    }

    override fun onBindViewHolder(holder: ProviderViewHolder, position: Int) {
        holder.bind(getItem(position))
    }
}

/**
 * View holder for a [Provider] item.
 */
internal class ProviderViewHolder(
    itemView: View,
    private val onProviderSelected: (Provider) -> Unit,
) : RecyclerView.ViewHolder(itemView), View.OnClickListener {
    internal lateinit var provider: Provider

    init {
        itemView.setOnClickListener(this)
    }

    fun bind(item: Provider) {
        provider = item

        (itemView as TextView)
            .text = item.name
    }

    override fun onClick(v: View?) {
        onProviderSelected(provider)
    }
}
