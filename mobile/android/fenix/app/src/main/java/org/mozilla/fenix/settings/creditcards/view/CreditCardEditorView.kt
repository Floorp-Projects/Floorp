/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.creditcards.view

import android.content.res.ColorStateList
import android.view.View
import android.widget.ArrayAdapter
import androidx.annotation.VisibleForTesting
import mozilla.components.concept.storage.CreditCardNumber
import mozilla.components.concept.storage.NewCreditCardFields
import mozilla.components.concept.storage.UpdatableCreditCardFields
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.support.ktx.android.view.hideKeyboard
import mozilla.components.support.utils.creditCardIIN
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.FragmentCreditCardEditorBinding
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
    private val binding: FragmentCreditCardEditorBinding,
    private val interactor: CreditCardEditorInteractor,
) {

    /**
     * Binds the given [CreditCardEditorState] in the [CreditCardEditorFragment].
     */
    fun bind(state: CreditCardEditorState) {
        if (state.isEditing) {
            binding.deleteButton.apply {
                visibility = View.VISIBLE

                setOnClickListener {
                    interactor.onDeleteCardButtonClicked(state.guid)
                }
            }
        }

        binding.cancelButton.setOnClickListener {
            interactor.onCancelButtonClicked()
        }

        binding.saveButton.setOnClickListener {
            saveCreditCard(state)
        }

        binding.cardNumberInput.text = state.cardNumber.toEditable()
        binding.nameOnCardInput.text = state.billingName.toEditable()

        binding.cardNumberLayout.setErrorTextColor(
            ColorStateList.valueOf(
                binding.root.context.getColorFromAttr(R.attr.textWarning),
            ),
        )
        binding.nameOnCardLayout.setErrorTextColor(
            ColorStateList.valueOf(
                binding.root.context.getColorFromAttr(R.attr.textWarning),
            ),
        )

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
        binding.root.hideKeyboard()

        if (validateForm()) {
            val cardNumber = binding.cardNumberInput.text.toString().toCreditCardNumber()

            if (state.isEditing) {
                val fields = UpdatableCreditCardFields(
                    billingName = binding.nameOnCardInput.text.toString(),
                    cardNumber = CreditCardNumber.Plaintext(cardNumber),
                    cardNumberLast4 = cardNumber.last4Digits(),
                    expiryMonth = (binding.expiryMonthDropDown.selectedItemPosition + 1).toLong(),
                    expiryYear = binding.expiryYearDropDown.selectedItem.toString().toLong(),
                    cardType = cardNumber.creditCardIIN()?.creditCardIssuerNetwork?.name ?: "",
                )
                interactor.onUpdateCreditCard(state.guid, fields)
            } else {
                val fields = NewCreditCardFields(
                    billingName = binding.nameOnCardInput.text.toString(),
                    plaintextCardNumber = CreditCardNumber.Plaintext(cardNumber),
                    cardNumberLast4 = cardNumber.last4Digits(),
                    expiryMonth = (binding.expiryMonthDropDown.selectedItemPosition + 1).toLong(),
                    expiryYear = binding.expiryYearDropDown.selectedItem.toString().toLong(),
                    cardType = cardNumber.creditCardIIN()?.creditCardIssuerNetwork?.name ?: "",
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

        if (binding.cardNumberInput.text.toString().validateCreditCardNumber()) {
            binding.cardNumberLayout.error = null
            binding.cardNumberTitle.setTextColor(binding.root.context.getColorFromAttr(R.attr.textPrimary))
        } else {
            isValid = false

            binding.cardNumberLayout.error =
                binding.root.context.getString(R.string.credit_cards_number_validation_error_message_2)
            binding.cardNumberTitle.setTextColor(binding.root.context.getColorFromAttr(R.attr.textWarning))
        }

        if (binding.nameOnCardInput.text.toString().isNotBlank()) {
            binding.nameOnCardLayout.error = null
            binding.nameOnCardTitle.setTextColor(binding.root.context.getColorFromAttr(R.attr.textPrimary))
        } else {
            isValid = false

            binding.nameOnCardLayout.error =
                binding.root.context.getString(R.string.credit_cards_name_on_card_validation_error_message_2)
            binding.nameOnCardTitle.setTextColor(binding.root.context.getColorFromAttr(R.attr.textWarning))
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
                binding.root.context,
                android.R.layout.simple_spinner_dropdown_item,
            )
        val dateFormat = SimpleDateFormat("MMMM (MM)", Locale.getDefault())

        val calendar = Calendar.getInstance()
        calendar.set(Calendar.DAY_OF_MONTH, 1)

        for (month in 0..NUMBER_OF_MONTHS) {
            calendar.set(Calendar.MONTH, month)
            adapter.add(dateFormat.format(calendar.time))
        }

        binding.expiryMonthDropDown.adapter = adapter
        binding.expiryMonthDropDown.setSelection(expiryMonth - 1)
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
                binding.root.context,
                android.R.layout.simple_spinner_dropdown_item,
            )
        val (startYear, endYear) = expiryYears

        for (year in startYear until endYear) {
            adapter.add(year.toString())
        }

        binding.expiryYearDropDown.adapter = adapter
    }

    companion object {
        // Number of months in a year (0-indexed).
        const val NUMBER_OF_MONTHS = 11
    }
}
