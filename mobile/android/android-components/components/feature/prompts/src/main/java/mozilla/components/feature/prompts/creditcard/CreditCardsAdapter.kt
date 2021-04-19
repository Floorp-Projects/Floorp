/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.creditcard

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import mozilla.components.concept.engine.prompt.CreditCard

/**
 * Adapter for a list of credit cards to be displayed.
 *
 * @param onCreditCardSelected Callback invoked when a credit card item is selected.
 */
class CreditCardsAdapter(
    private val onCreditCardSelected: (CreditCard) -> Unit
) : ListAdapter<CreditCard, CreditCardItemViewHolder>(DiffCallback) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): CreditCardItemViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(CreditCardItemViewHolder.LAYOUT_ID, parent, false)
        return CreditCardItemViewHolder(view, onCreditCardSelected)
    }

    override fun onBindViewHolder(holder: CreditCardItemViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    internal object DiffCallback : DiffUtil.ItemCallback<CreditCard>() {
        override fun areItemsTheSame(oldItem: CreditCard, newItem: CreditCard) =
            oldItem.guid == newItem.guid

        override fun areContentsTheSame(oldItem: CreditCard, newItem: CreditCard) =
            oldItem == newItem
    }
}
