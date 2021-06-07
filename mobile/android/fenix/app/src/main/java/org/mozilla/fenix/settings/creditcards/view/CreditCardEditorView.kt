/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.creditcards.view

import android.view.View
import android.widget.ArrayAdapter
import androidx.annotation.VisibleForTesting
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.fragment_credit_card_editor.*
import mozilla.components.concept.storage.CreditCardNumber
import mozilla.components.concept.storage.NewCreditCardFields
import mozilla.components.concept.storage.UpdatableCreditCardFields
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.support.ktx.android.view.hideKeyboard
import mozilla.components.support.utils.creditCardIIN
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.toEditable
import org.mozilla.fenix.settings.creditcards.CreditCardEditorFragment
import org.mozilla.fenix.settings.creditcards.CreditCardEditorState
import org.mozilla.fenix.settings.creditcards.interactor.CreditCardEditorInteractor
import org.mozilla.fenix.settings.creditcards.last4Digits
import org.mozilla.fenix.settings.creditcards.toCreditCardNumber
import org.mozilla.fenix.settings.creditcards.validateCreditCardNumber
import java.text.SimpleDateFormat
import java.util.Calendar
import java.util.Locale

/**
 * Shows a credit card editor for adding or updating a credit card.
 */
class CreditCardEditorView(
    override val containerView: View,
    private val interactor: CreditCardEditorInteractor
) : LayoutContainer {

    /**
     * Binds the given [CreditCardEditorState] in the [CreditCardEditorFragment].
     */
    fun bind(state: CreditCardEditorState) {
        if (state.isEditing) {
            delete_button.apply {
                visibility = View.VISIBLE

                setOnClickListener {
                    interactor.onDeleteCardButtonClicked(state.guid)
                }
            }
        }

        cancel_button.setOnClickListener {
            interactor.onCancelButtonClicked()
        }

        save_button.setOnClickListener {
            saveCreditCard(state)
        }

        card_number_input.text = state.cardNumber.toEditable()
        name_on_card_input.text = state.billingName.toEditable()

        bindExpiryMonthDropDown(state.expiryMonth)
        bindExpiryYearDropDown(state.expiryYears)
    }

    /**
     * Saves a new credit card or updates an existing one with data from the user input.
     *
     * @param state The state of the [CreditCardEditorFragment] containing the edited credit card
     * information.
     */
    internal fun saveCreditCard(state: CreditCardEditorState) {
        containerView.hideKeyboard()

        if (validateForm()) {
            val cardNumber = card_number_input.text.toString().toCreditCardNumber()

            if (state.isEditing) {
                val fields = UpdatableCreditCardFields(
                    billingName = name_on_card_input.text.toString(),
                    cardNumber = CreditCardNumber.Plaintext(cardNumber),
                    cardNumberLast4 = cardNumber.last4Digits(),
                    expiryMonth = (expiry_month_drop_down.selectedItemPosition + 1).toLong(),
                    expiryYear = expiry_year_drop_down.selectedItem.toString().toLong(),
                    cardType = cardNumber.creditCardIIN()?.creditCardIssuerNetwork?.name ?: ""
                )
                interactor.onUpdateCreditCard(state.guid, fields)
            } else {
                val fields = NewCreditCardFields(
                    billingName = name_on_card_input.text.toString(),
                    plaintextCardNumber = CreditCardNumber.Plaintext(cardNumber),
                    cardNumberLast4 = cardNumber.last4Digits(),
                    expiryMonth = (expiry_month_drop_down.selectedItemPosition + 1).toLong(),
                    expiryYear = expiry_year_drop_down.selectedItem.toString().toLong(),
                    cardType = cardNumber.creditCardIIN()?.creditCardIssuerNetwork?.name ?: ""
                )
                interactor.onSaveCreditCard(fields)
            }
        }
    }

    /**
     * Validates the credit card information entered by the user.
     *
     * @return true if the credit card information is valid, false otherwise.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun validateForm(): Boolean {
        var isValid = true

        if (card_number_input.text.toString().validateCreditCardNumber()) {
            card_number_layout.error = null
            card_number_title.setTextColor(containerView.context.getColorFromAttr(R.attr.primaryText))
        } else {
            isValid = false

            card_number_layout.error =
                containerView.context.getString(R.string.credit_cards_number_validation_error_message)
            card_number_title.setTextColor(containerView.context.getColorFromAttr(R.attr.destructive))
        }

        if (name_on_card_input.text.toString().isNotBlank()) {
            name_on_card_layout.error = null
            name_on_card_title.setTextColor(containerView.context.getColorFromAttr(R.attr.primaryText))
        } else {
            isValid = false

            name_on_card_layout.error =
                containerView.context.getString(R.string.credit_cards_name_on_card_validation_error_message)
            name_on_card_title.setTextColor(containerView.context.getColorFromAttr(R.attr.destructive))
        }

        return isValid
    }

    /**
     * Setup the expiry month dropdown by formatting and populating it with the months in a calendar
     * year, and set the selection to the provided expiry month.
     *
     * @param expiryMonth The selected credit card expiry month to display.
     */
    private fun bindExpiryMonthDropDown(expiryMonth: Int) {
        val adapter =
            ArrayAdapter<String>(
                containerView.context,
                android.R.layout.simple_spinner_dropdown_item
            )
        val dateFormat = SimpleDateFormat("MMMM (MM)", Locale.getDefault())

        val calendar = Calendar.getInstance()
        calendar.set(Calendar.DAY_OF_MONTH, 1)

        for (month in 0..NUMBER_OF_MONTHS) {
            calendar.set(Calendar.MONTH, month)
            adapter.add(dateFormat.format(calendar.time))
        }

        expiry_month_drop_down.adapter = adapter
        expiry_month_drop_down.setSelection(expiryMonth - 1)
    }

    /**
     * Setup the expiry year dropdown with the range specified by the provided expiryYears
     *
     * @param expiryYears A range specifying the start and end year to display in the expiry year
     * dropdown.
     */
    private fun bindExpiryYearDropDown(expiryYears: Pair<Int, Int>) {
        val adapter =
            ArrayAdapter<String>(
                containerView.context,
                android.R.layout.simple_spinner_dropdown_item
            )
        val (startYear, endYear) = expiryYears

        for (year in startYear until endYear) {
            adapter.add(year.toString())
        }

        expiry_year_drop_down.adapter = adapter
    }

    companion object {
        // Number of months in a year (0-indexed).
        const val NUMBER_OF_MONTHS = 11
    }
}
