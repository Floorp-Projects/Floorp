/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.creditcard

import android.view.View
import android.widget.ImageView
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.concept.storage.CreditCardEntry
import mozilla.components.feature.prompts.R
import mozilla.components.support.utils.creditCardIssuerNetwork

/**
 * View holder for displaying a credit card item.
 *
 * @param onCreditCardSelected Callback invoked when a credit card item is selected.
 */
class CreditCardItemViewHolder(
    view: View,
    private val onCreditCardSelected: (CreditCardEntry) -> Unit,
) : RecyclerView.ViewHolder(view) {

    /**
     * Binds the view with the provided [CreditCardEntry].
     *
     * @param creditCard The [CreditCardEntry] to display.
     */
    fun bind(creditCard: CreditCardEntry) {
        itemView.findViewById<ImageView>(R.id.credit_card_logo)
            .setImageResource(creditCard.cardType.creditCardIssuerNetwork().icon)

        itemView.findViewById<TextView>(R.id.credit_card_number).text =
            creditCard.obfuscatedCardNumber

        itemView.findViewById<TextView>(R.id.credit_card_expiration_date).text =
            creditCard.expiryDate

        itemView.setOnClickListener {
            onCreditCardSelected(creditCard)
        }
    }

    companion object {
        val LAYOUT_ID = R.layout.mozac_feature_prompts_credit_card_list_item
    }
}
