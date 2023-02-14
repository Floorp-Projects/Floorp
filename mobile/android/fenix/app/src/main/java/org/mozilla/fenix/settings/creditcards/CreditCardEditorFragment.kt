/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.creditcards

import android.annotation.SuppressLint
import android.content.DialogInterface
import android.os.Bundle
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import androidx.appcompat.app.AlertDialog
import androidx.core.view.MenuProvider
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.support.ktx.android.view.hideKeyboard
import mozilla.components.support.ktx.android.view.showKeyboard
import org.mozilla.fenix.R
import org.mozilla.fenix.SecureFragment
import org.mozilla.fenix.databinding.FragmentCreditCardEditorBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.placeCursorAtEnd
import org.mozilla.fenix.ext.redirectToReAuth
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.settings.creditcards.controller.DefaultCreditCardEditorController
import org.mozilla.fenix.settings.creditcards.interactor.CreditCardEditorInteractor
import org.mozilla.fenix.settings.creditcards.interactor.DefaultCreditCardEditorInteractor
import org.mozilla.fenix.settings.creditcards.view.CreditCardEditorView

/**
 * Display a credit card editor for adding and editing a credit card.
 */
class CreditCardEditorFragment :
    SecureFragment(R.layout.fragment_credit_card_editor),
    MenuProvider {

    private lateinit var creditCardEditorState: CreditCardEditorState
    private lateinit var creditCardEditorView: CreditCardEditorView
    private lateinit var menu: Menu

    private var deleteDialog: AlertDialog? = null

    private val args by navArgs<CreditCardEditorFragmentArgs>()

    /**
     * Returns true if a credit card is being edited, and false otherwise.
     */
    private val isEditing: Boolean
        get() = args.creditCard != null

    private lateinit var interactor: CreditCardEditorInteractor

    @SuppressLint("ClickableViewAccessibility")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        requireActivity().addMenuProvider(this, viewLifecycleOwner, Lifecycle.State.RESUMED)

        val storage = requireContext().components.core.autofillStorage
        interactor = DefaultCreditCardEditorInteractor(
            controller = DefaultCreditCardEditorController(
                storage = storage,
                lifecycleScope = lifecycleScope,
                navController = findNavController(),
                showDeleteDialog = ::showDeleteDialog,
            ),
        )

        val binding = FragmentCreditCardEditorBinding.bind(view)

        lifecycleScope.launch(Dispatchers.Main) {
            creditCardEditorState = withContext(Dispatchers.IO) {
                args.creditCard?.toCreditCardEditorState(storage)
                    ?: getInitialCreditCardEditorState()
            }
            creditCardEditorView = CreditCardEditorView(binding, interactor)
            creditCardEditorView.bind(creditCardEditorState)

            binding.apply {
                cardNumberInput.apply {
                    requestFocus()
                    placeCursorAtEnd()
                    showKeyboard()
                }
                expiryMonthDropDown.setOnTouchListener { view, _ ->
                    view?.hideKeyboard()
                    false
                }
                expiryYearDropDown.setOnTouchListener { view, _ ->
                    view?.hideKeyboard()
                    false
                }
            }
        }
    }

    override fun onResume() {
        super.onResume()
        if (!isEditing) {
            showToolbar(getString(R.string.credit_cards_add_card))
        } else {
            showToolbar(getString(R.string.credit_cards_edit_card))
        }
    }

    /**
     * Close the keyboard, any open dialogs or menus and then reauthenticate if the
     * fragment is paused and the user is not navigating to [CreditCardsManagementFragment].
     */
    override fun onPause() {
        view?.hideKeyboard()
        menu.close()
        deleteDialog?.dismiss()

        redirectToReAuth(
            listOf(R.id.creditCardsManagementFragment),
            findNavController().currentDestination?.id,
            R.id.creditCardEditorFragment,
        )

        super.onPause()
    }

    override fun onCreateMenu(menu: Menu, inflater: MenuInflater) {
        inflater.inflate(R.menu.credit_card_editor, menu)
        this.menu = menu

        menu.findItem(R.id.delete_credit_card_button).isVisible = isEditing
    }

    @Suppress("MagicNumber")
    override fun onMenuItemSelected(item: MenuItem): Boolean = when (item.itemId) {
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

    private fun showDeleteDialog(onPositiveClickListener: DialogInterface.OnClickListener) {
        deleteDialog = AlertDialog.Builder(requireContext()).apply {
            setMessage(R.string.credit_cards_delete_dialog_confirmation)
            setNegativeButton(R.string.credit_cards_cancel_button) { dialog: DialogInterface, _ ->
                dialog.cancel()
            }
            setPositiveButton(R.string.credit_cards_delete_dialog_button, onPositiveClickListener)
            create()
        }.show()
    }

    companion object {
        // Number of years to show in the expiry year dropdown.
        const val NUMBER_OF_YEARS_TO_SHOW = 10
    }
}
