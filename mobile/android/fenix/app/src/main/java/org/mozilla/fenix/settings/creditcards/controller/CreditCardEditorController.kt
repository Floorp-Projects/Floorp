/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.creditcards.controller

import androidx.navigation.NavController
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.concept.storage.NewCreditCardFields
import mozilla.components.concept.storage.UpdatableCreditCardFields
import mozilla.components.service.sync.autofill.AutofillCreditCardsAddressesStorage
import org.mozilla.fenix.settings.creditcards.CreditCardEditorFragment
import org.mozilla.fenix.settings.creditcards.interactor.CreditCardEditorInteractor
import org.mozilla.fenix.utils.Settings

/**
 * [CreditCardEditorFragment] controller. An interface that handles the view manipulation of the
 * credit card editor.
 */
interface CreditCardEditorController {

    /**
     * @see [CreditCardEditorInteractor.onCancelButtonClicked]
     */
    fun handleCancelButtonClicked()

    /**
     * @see [CreditCardEditorInteractor.onDeleteCardButtonClicked]
     */
    fun handleDeleteCreditCard(guid: String)

    /**
     * @see [CreditCardEditorInteractor.onSaveCreditCard]
     */
    fun handleSaveCreditCard(creditCardFields: NewCreditCardFields)

    /**
     * @see [CreditCardEditorInteractor.onUpdateCreditCard]
     */
    fun handleUpdateCreditCard(guid: String, creditCardFields: UpdatableCreditCardFields)
}

/**
 * The default implementation of [CreditCardEditorController].
 *
 * @param storage An instance of the [AutofillCreditCardsAddressesStorage] for adding and retrieving
 * credit cards.
 * @param lifecycleScope [CoroutineScope] scope to launch coroutines.
 * @param navController [NavController] used for navigation.
 * @param settings [Settings] application settings.
 * @param ioDispatcher [CoroutineDispatcher] used for executing async tasks. Defaults to [Dispatchers.IO].
 */
class DefaultCreditCardEditorController(
    private val storage: AutofillCreditCardsAddressesStorage,
    private val lifecycleScope: CoroutineScope,
    private val navController: NavController,
    private val settings: Settings,
    private val ioDispatcher: CoroutineDispatcher = Dispatchers.IO
) : CreditCardEditorController {

    override fun handleCancelButtonClicked() {
        navController.popBackStack()
    }

    override fun handleDeleteCreditCard(guid: String) {
        settings.creditCardsDeletedCount += 1

        lifecycleScope.launch(ioDispatcher) {
            storage.deleteCreditCard(guid)

            lifecycleScope.launch(Dispatchers.Main) {
                navController.popBackStack()
            }
        }
    }

    override fun handleSaveCreditCard(creditCardFields: NewCreditCardFields) {
        settings.creditCardsSavedCount += 1

        lifecycleScope.launch(ioDispatcher) {
            storage.addCreditCard(creditCardFields)

            lifecycleScope.launch(Dispatchers.Main) {
                navController.popBackStack()
            }
        }
    }

    override fun handleUpdateCreditCard(guid: String, creditCardFields: UpdatableCreditCardFields) {
        lifecycleScope.launch(ioDispatcher) {
            storage.updateCreditCard(guid, creditCardFields)

            lifecycleScope.launch(Dispatchers.Main) {
                navController.popBackStack()
            }
        }
    }
}
