/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.creditcards.view

import android.view.View
import kotlinx.android.synthetic.main.credit_card_list_item.*
import mozilla.components.concept.storage.CreditCard
import mozilla.components.support.utils.creditCardIssuerNetwork
import org.mozilla.fenix.R
import org.mozilla.fenix.settings.creditcards.interactor.CreditCardsManagementInteractor
import org.mozilla.fenix.utils.view.ViewHolder
import java.text.SimpleDateFormat
import java.util.Calendar
import java.util.Locale

/**
 * View holder for a credit card list item.
 */
class CreditCardItemViewHolder(
    view: View,
    private val interactor: CreditCardsManagementInteractor
) : ViewHolder(view) {

    fun bind(creditCard: CreditCard) {
        credit_card_logo.setImageResource(creditCard.cardType.creditCardIssuerNetwork().icon)

        credit_card_number.text = creditCard.obfuscatedCardNumber

        bindCreditCardExpiryDate(creditCard)

        itemView.setOnClickListener {
            interactor.onSelectCreditCard(creditCard)
        }
    }

    /**
     * Set the credit card expiry date formatted according to the locale.
     */
    private fun bindCreditCardExpiryDate(creditCard: CreditCard) {
        val dateFormat = SimpleDateFormat(DATE_PATTERN, Locale.getDefault())

        val calendar = Calendar.getInstance()
        calendar.set(Calendar.DAY_OF_MONTH, 1)
        // Subtract 1 from the expiry month since Calendar.Month is based on a 0-indexed.
        calendar.set(Calendar.MONTH, creditCard.expiryMonth.toInt() - 1)
        calendar.set(Calendar.YEAR, creditCard.expiryYear.toInt())

        expiry_date.text = dateFormat.format(calendar.time)
    }

    companion object {
        const val LAYOUT_ID = R.layout.credit_card_list_item

        // Date format pattern for the credit card expiry date.
        private const val DATE_PATTERN = "MM/yyyy"
    }
}
