/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.identitycredential

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.TextView
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.concept.identitycredential.Account
import mozilla.components.feature.prompts.R
import mozilla.components.support.ktx.kotlin.base64PngToBitmap

private object AccountItemDiffCallback : DiffUtil.ItemCallback<Account>() {
    override fun areItemsTheSame(oldItem: Account, newItem: Account) =
        oldItem.id == newItem.id

    override fun areContentsTheSame(oldItem: Account, newItem: Account) =
        oldItem == newItem
}

/**
 * RecyclerView adapter for displaying account items.
 */
internal class BasicAccountAdapter(
    private val onAccountSelected: (Account) -> Unit,
) : ListAdapter<Account, AccountViewHolder>(AccountItemDiffCallback) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): AccountViewHolder {
        val view = LayoutInflater
            .from(parent.context)
            .inflate(
                R.layout.mozac_feature_prompts_identity_crendential_account_item,
                parent,
                false,
            )
        return AccountViewHolder(view, onAccountSelected)
    }

    override fun onBindViewHolder(holder: AccountViewHolder, position: Int) {
        holder.bind(getItem(position))
    }
}

/**
 * View holder for a account item.
 */
internal class AccountViewHolder(
    itemView: View,
    private val onAccountSelected: (Account) -> Unit,
) : RecyclerView.ViewHolder(itemView), View.OnClickListener {
    internal lateinit var account: Account

    init {
        itemView.setOnClickListener(this)
    }

    fun bind(item: Account) {
        account = item

        itemView.findViewById<TextView>(R.id.fedcm_account_name).text = item.name

        account.icon?.let {
            itemView.findViewById<ImageView>(R.id.fedcm_account_icon)
                .setImageBitmap(it.base64PngToBitmap())
        }
    }

    override fun onClick(v: View?) {
        onAccountSelected(account)
    }
}
