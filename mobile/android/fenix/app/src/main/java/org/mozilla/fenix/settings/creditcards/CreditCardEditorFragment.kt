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
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean = when (item.itemId) {
        R.id.save_credit_card_button -> {
            saveCreditCard()
            true
        }
        else -> false
    }

    /**
     * Helper function called by the the "Save" button and menu item to save a new credit card
     * from the entered credit card fields.
     */
    private fun saveCreditCard() {
        view?.hideKeyboard()

        interactor.onSaveButtonClicked(
            UpdatableCreditCardFields(
                billingName = name_on_card_input.text.toString(),
                cardNumber = card_number_input.text.toString(),
                expiryMonth = (expiry_month_drop_down.selectedItemPosition + 1).toLong(),
                expiryYear = expiry_year_drop_down.selectedItem.toString().toLong(),
                cardType = "amex"
            )
        )
    }

    companion object {
        // Number of years to show in the expiry year dropdown.
        const val NUMBER_OF_YEARS_TO_SHOW = 10
    }
}
