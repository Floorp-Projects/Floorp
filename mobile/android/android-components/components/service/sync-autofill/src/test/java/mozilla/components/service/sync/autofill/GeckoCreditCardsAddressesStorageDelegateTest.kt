/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.sync.autofill

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.TestCoroutineScope
import mozilla.components.concept.storage.CreditCardNumber
import mozilla.components.concept.storage.NewCreditCardFields
import mozilla.components.lib.dataprotect.SecureAbove22Preferences
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class GeckoCreditCardsAddressesStorageDelegateTest {

    private lateinit var storage: AutofillCreditCardsAddressesStorage
    private lateinit var securePrefs: SecureAbove22Preferences
    private lateinit var delegate: GeckoCreditCardsAddressesStorageDelegate
    private lateinit var scope: TestCoroutineScope

    init {
        testContext.getDatabasePath(AUTOFILL_DB_NAME)!!.parentFile!!.mkdirs()
    }

    @Before
    fun before() = runBlocking {
        scope = TestCoroutineScope()
        // forceInsecure is set in the tests because a keystore wouldn't be configured in the test environment.
        securePrefs = SecureAbove22Preferences(testContext, "autofill", forceInsecure = true)
        storage = AutofillCreditCardsAddressesStorage(testContext, lazy { securePrefs })
        delegate = GeckoCreditCardsAddressesStorageDelegate(lazy { storage }, scope)
    }

    @Test
    fun `decrypt`() = runBlocking {
        val plaintextNumber = CreditCardNumber.Plaintext("4111111111111111")
        val creditCardFields = NewCreditCardFields(
            billingName = "Jon Doe",
            plaintextCardNumber = plaintextNumber,
            cardNumberLast4 = "1111",
            expiryMonth = 12,
            expiryYear = 2028,
            cardType = "amex"
        )
        val creditCard = storage.addCreditCard(creditCardFields)

        assertEquals(
            plaintextNumber,
            delegate.decrypt(creditCard.encryptedCardNumber)
        )
    }

    @Test
    fun `onAddressFetch`() {
        scope.launch {
            delegate.onAddressesFetch()
            verify(storage, times(1)).getAllAddresses()
        }
    }

    @Test
    fun `onCreditCardFetch`() {
        scope.launch {
            delegate.onCreditCardsFetch()
            verify(storage, times(1)).getAllCreditCards()
        }
    }
}
