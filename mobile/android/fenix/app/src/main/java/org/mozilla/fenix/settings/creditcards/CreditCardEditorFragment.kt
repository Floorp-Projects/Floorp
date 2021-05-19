/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.creditcards

import android.os.Bundle
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import org.mozilla.fenix.R
import org.mozilla.fenix.SecureFragment
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.settings.creditcards.controller.DefaultCreditCardEditorController
import org.mozilla.fenix.settings.creditcards.interactor.CreditCardEditorInteractor
import org.mozilla.fenix.settings.creditcards.interactor.DefaultCreditCardEditorInteractor
import org.mozilla.fenix.settings.creditcards.view.CreditCardEditorView

/**
 * Display a credit card editor for adding and editing a credit card.
 */
class CreditCardEditorFragment : SecureFragment(R.layout.fragment_credit_card_editor) {

    private lateinit var creditCardEditorState: CreditCardEditorState
    private lateinit var creditCardEditorView: CreditCardEditorView
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

        creditCardEditorState =
            args.creditCard?.toCreditCardEditorState() ?: getInitialCreditCardEditorState()
        creditCardEditorView = CreditCardEditorView(view, interactor)
        creditCardEditorView.bind(creditCardEditorState)
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
            creditCardEditorView.saveCreditCard(creditCardEditorState)
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
