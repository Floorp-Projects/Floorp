/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.address

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.annotation.VisibleForTesting
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.concept.storage.Address
import mozilla.components.feature.prompts.R

@VisibleForTesting
internal object AddressDiffCallback : DiffUtil.ItemCallback<Address>() {
    override fun areItemsTheSame(oldItem: Address, newItem: Address) =
        oldItem.guid == newItem.guid

    override fun areContentsTheSame(oldItem: Address, newItem: Address) =
        oldItem == newItem
}

/**
 * RecyclerView adapter for displaying address items.
 */
internal class AddressAdapter(
    private val onAddressSelected: (Address) -> Unit
) : ListAdapter<Address, AddressViewHolder>(AddressDiffCallback) {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): AddressViewHolder {
        val view = LayoutInflater
            .from(parent.context)
            .inflate(R.layout.mozac_feature_prompts_address_list_item, parent, false)
        return AddressViewHolder(view, onAddressSelected)
    }

    override fun onBindViewHolder(holder: AddressViewHolder, position: Int) {
        holder.bind(getItem(position))
    }
}

/**
 * View holder for a address item.
 */
@VisibleForTesting
internal class AddressViewHolder(
    itemView: View,
    private val onAddressSelected: (Address) -> Unit
) : RecyclerView.ViewHolder(itemView), View.OnClickListener {
    @VisibleForTesting
    lateinit var address: Address

    init {
        itemView.setOnClickListener(this)
    }

    fun bind(address: Address) {
        this.address = address
        itemView.findViewById<TextView>(R.id.address_name)?.text = address.addressLabel
    }

    override fun onClick(v: View?) {
        onAddressSelected(address)
    }
}
