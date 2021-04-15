/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.creditcards

import android.os.Bundle
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import kotlinx.android.synthetic.main.fragment_credit_card_editor.*
import mozilla.components.concept.storage.CreditCardNumber
import mozilla.components.concept.storage.NewCreditCardFields
import mozilla.components.concept.storage.UpdatableCreditCardFields
import mozilla.components.support.ktx.android.view.hideKeyboard
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.settings.creditcards.controller.DefaultCreditCardEditorController
import org.mozilla.fenix.settings.creditcards.interactor.CreditCardEditorInteractor
import org.mozilla.fenix.settings.creditcards.interactor.DefaultCreditCardEditorInteractor
import org.mozilla.fenix.settings.creditcards.view.CreditCardEditorView

/**
 * Display a credit card editor for adding and editing a credit card.
 */
class CreditCardEditorFragment : Fragment(R.layout.fragment_credit_card_editor) {

    private val args by navArgs<CreditCardEditorFragmentArgs>()

    /**
     * Returns true if a credit card is being edited, and false otherwise.
     */
    private val isEditing: Boolean
        get() = args.creditCard != null

    private lateinit var interactor: CreditCardEditorInteractor

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        setHasOptionsMenu(true)

        if (!isEditing) {
            showToolbar(getString(R.string.credit_cards_add_card))
        } else {
            showToolbar(getString(R.string.credit_cards_edit_card))
        }

        interactor = DefaultCreditCardEditorInteractor(
            controller = DefaultCreditCardEditorController(
                storage = requireContext().components.core.autofillStorage,
                lifecycleScope = lifecycleScope,
                navController = findNavController()
            )
        )

        val creditCardEditorState =
            args.creditCard?.toCreditCardEditorState() ?: getInitialCreditCardEditorState()
        CreditCardEditorView(view, interactor).bind(creditCardEditorState)
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        inflater.inflate(R.menu.credit_card_editor, menu)

        menu.findItem(R.id.delete_credit_card_button).isVisible = isEditing
    }

    @Suppress("MagicNumber")
    override fun onOptionsItemSelected(item: MenuItem): Boolean = when (item.itemId) {
        R.id.delete_credit_card_button -> {
            args.creditCard?.let { interactor.onDeleteCardButtonClicked(it.guid) }
            true
        }
        R.id.save_credit_card_button -> {
            view?.hideKeyboard()

            val creditCard = args.creditCard

            // TODO need to know if we're updating a number, or just round-tripping it
            val cardNumber = card_number_input.text.toString()

            if (creditCard != null) {
                val fields = UpdatableCreditCardFields(
                        billingName = name_on_card_input.text.toString(),
                        cardNumber = CreditCardNumber.Plaintext(cardNumber),
                        cardNumberLast4 = cardNumber.substring(cardNumber.length - 5, cardNumber.length - 1),
                        expiryMonth = (expiry_month_drop_down.selectedItemPosition + 1).toLong(),
                        expiryYear = expiry_year_drop_down.selectedItem.toString().toLong(),
                        cardType = CARD_TYPE_PLACEHOLDER
                    )
                interactor.onUpdateCreditCard(creditCard.guid, fields)
            } else {
                val fields = NewCreditCardFields(
                    billingName = name_on_card_input.text.toString(),
                    plaintextCardNumber = CreditCardNumber.Plaintext(cardNumber),
                    cardNumberLast4 = cardNumber.substring(cardNumber.length - 5, cardNumber.length - 1),
                    expiryMonth = (expiry_month_drop_down.selectedItemPosition + 1).toLong(),
                    expiryYear = expiry_year_drop_down.selectedItem.toString().toLong(),
                    cardType = CARD_TYPE_PLACEHOLDER
                )
                interactor.onSaveCreditCard(fields)
            }

            true
        }
        else -> false
    }

    companion object {
        // Number of years to show in the expiry year dropdown.
        const val NUMBER_OF_YEARS_TO_SHOW = 10

        // Placeholder for the card type. This will be replaced when we can identify the card type.
        // This is dependent on https://github.com/mozilla-mobile/android-components/issues/9813.
        const val CARD_TYPE_PLACEHOLDER = ""
    }
}
