/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.creditcards.view

import android.R
import android.view.View
import android.widget.ArrayAdapter
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.fragment_credit_card_editor.*
import mozilla.components.concept.storage.CreditCardNumber
import mozilla.components.concept.storage.NewCreditCardFields
import mozilla.components.concept.storage.UpdatableCreditCardFields
import mozilla.components.support.ktx.android.view.hideKeyboard
import org.mozilla.fenix.ext.toEditable
import org.mozilla.fenix.settings.creditcards.CreditCardEditorFragment.Companion.CARD_TYPE_PLACEHOLDER
import org.mozilla.fenix.settings.creditcards.CreditCardEditorState
import org.mozilla.fenix.settings.creditcards.interactor.CreditCardEditorInteractor
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
            saveCreditCardInfo(state)
        }

        card_number_input.text = state.cardNumber.toEditable()
        name_on_card_input.text = state.billingName.toEditable()

        bindExpiryMonthDropDown(state.expiryMonth)
        bindExpiryYearDropDown(state.expiryYears)
    }

    @Suppress("MagicNumber")
    internal fun saveCreditCardInfo(state: CreditCardEditorState) {
        containerView.hideKeyboard()

        // TODO need to know if we're updating a number, or just round-tripping it
        val cardNumber = card_number_input.text.toString()
        if (state.isEditing) {
            val fields = UpdatableCreditCardFields(
                billingName = name_on_card_input.text.toString(),
                cardNumber = CreditCardNumber.Encrypted(cardNumber),
                cardNumberLast4 = cardNumber.substring(cardNumber.length - 4),
                expiryMonth = (expiry_month_drop_down.selectedItemPosition + 1).toLong(),
                expiryYear = expiry_year_drop_down.selectedItem.toString().toLong(),
                cardType = CARD_TYPE_PLACEHOLDER
            )
            interactor.onUpdateCreditCard(state.guid, fields)
        } else {
            val fields = NewCreditCardFields(
                billingName = name_on_card_input.text.toString(),
                plaintextCardNumber = CreditCardNumber.Plaintext(cardNumber),
                cardNumberLast4 = cardNumber.substring(cardNumber.length - 4),
                expiryMonth = (expiry_month_drop_down.selectedItemPosition + 1).toLong(),
                expiryYear = expiry_year_drop_down.selectedItem.toString().toLong(),
                cardType = CARD_TYPE_PLACEHOLDER
            )
            interactor.onSaveCreditCard(fields)
        }
    }

    /**
     * Setup the expiry month dropdown by formatting and populating it with the months in a calendar
     * year, and set the selection to the provided expiry month.
     *
     * @param expiryMonth The selected credit card expiry month to display.
     */
    private fun bindExpiryMonthDropDown(expiryMonth: Int) {
        val adapter =
            ArrayAdapter<String>(containerView.context, R.layout.simple_spinner_dropdown_item)
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
            ArrayAdapter<String>(containerView.context, R.layout.simple_spinner_dropdown_item)
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
