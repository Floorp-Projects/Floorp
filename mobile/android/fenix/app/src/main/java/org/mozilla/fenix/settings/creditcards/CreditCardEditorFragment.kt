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
import org.mozilla.fenix.ext.redirectToReAuth
import org.mozilla.fenix.ext.settings
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
    private lateinit var menu: Menu

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

        val storage = requireContext().components.core.autofillStorage
        interactor = DefaultCreditCardEditorInteractor(
            controller = DefaultCreditCardEditorController(
                storage = storage,
                lifecycleScope = lifecycleScope,
                navController = findNavController(),
                settings = requireContext().settings()
            )
        )

        creditCardEditorState =
            args.creditCard?.toCreditCardEditorState(storage) ?: getInitialCreditCardEditorState()
        creditCardEditorView = CreditCardEditorView(view, interactor)
        creditCardEditorView.bind(creditCardEditorState)
    }

    /**
     * Close any open dialogs or menus and reauthenticate if the fragment is paused and
     * the user is not navigating to [CreditCardsManagementFragment].
     */
    override fun onPause() {
        menu.close()

        redirectToReAuth(
            listOf(R.id.creditCardsManagementFragment),
            findNavController().currentDestination?.id,
            R.id.creditCardEditorFragment
        )

        super.onPause()
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        inflater.inflate(R.menu.credit_card_editor, menu)
        this.menu = menu

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
    }
}
