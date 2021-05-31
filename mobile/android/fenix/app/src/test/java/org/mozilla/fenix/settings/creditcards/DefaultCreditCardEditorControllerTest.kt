/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.creditcards

import androidx.navigation.NavController
import io.mockk.coVerify
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.TestCoroutineScope
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.concept.storage.CreditCardNumber
import mozilla.components.concept.storage.NewCreditCardFields
import mozilla.components.concept.storage.UpdatableCreditCardFields
import mozilla.components.service.sync.autofill.AutofillCreditCardsAddressesStorage
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.utils.CreditCardNetworkType
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.creditcards.controller.DefaultCreditCardEditorController
import org.mozilla.fenix.utils.Settings

@ExperimentalCoroutinesApi
@RunWith(FenixRobolectricTestRunner::class)
class DefaultCreditCardEditorControllerTest {

    private val storage: AutofillCreditCardsAddressesStorage = mockk(relaxed = true)
    private val navController: NavController = mockk(relaxed = true)

    private val testCoroutineScope = TestCoroutineScope()
    private val testDispatcher = TestCoroutineDispatcher()

    private lateinit var controller: DefaultCreditCardEditorController
    private lateinit var settings: Settings

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(testDispatcher)

    @Before
    fun setup() {
        settings = Settings(testContext)
        controller = spyk(
            DefaultCreditCardEditorController(
                storage = storage,
                lifecycleScope = testCoroutineScope,
                navController = navController,
                settings = settings,
                ioDispatcher = testDispatcher
            )
        )
    }

    @After
    fun cleanUp() {
        testCoroutineScope.cleanupTestCoroutines()
        testDispatcher.cleanupTestCoroutines()
    }

    @Test
    fun handleCancelButtonClicked() {
        controller.handleCancelButtonClicked()

        verify {
            navController.popBackStack()
        }
    }

    @Test
    fun handleDeleteCreditCard() = testCoroutineScope.runBlockingTest {
        assertEquals(0, settings.creditCardsDeletedCount)

        val creditCardId = "id"

        controller.handleDeleteCreditCard(creditCardId)

        assertEquals(1, settings.creditCardsDeletedCount)

        coVerify {
            storage.deleteCreditCard(creditCardId)
            navController.popBackStack()
        }
    }

    @Test
    fun handleSaveCreditCard() = testCoroutineScope.runBlockingTest {
        assertEquals(0, settings.creditCardsSavedCount)

        val creditCardFields = NewCreditCardFields(
            billingName = "Banana Apple",
            plaintextCardNumber = CreditCardNumber.Plaintext("4111111111111112"),
            cardNumberLast4 = "1112",
            expiryMonth = 1,
            expiryYear = 2030,
            cardType = CreditCardNetworkType.DISCOVER.cardName
        )

        controller.handleSaveCreditCard(creditCardFields)

        assertEquals(1, settings.creditCardsSavedCount)

        coVerify {
            storage.addCreditCard(creditCardFields)
            navController.popBackStack()
        }
    }

    @Test
    fun handleUpdateCreditCard() = testCoroutineScope.runBlockingTest {
        val creditCardId = "id"
        val creditCardFields = UpdatableCreditCardFields(
            billingName = "Banana Apple",
            cardNumber = CreditCardNumber.Plaintext("4111111111111112"),
            cardNumberLast4 = "1112",
            expiryMonth = 1,
            expiryYear = 2034,
            cardType = CreditCardNetworkType.DISCOVER.cardName
        )

        controller.handleUpdateCreditCard(creditCardId, creditCardFields)

        coVerify {
            storage.updateCreditCard(creditCardId, creditCardFields)
            navController.popBackStack()
        }
    }
}
