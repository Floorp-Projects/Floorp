/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.sync.autofill

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.storage.CreditCard
import mozilla.components.concept.storage.CreditCardNumber
import mozilla.components.concept.storage.NewCreditCardFields
import mozilla.components.concept.storage.UpdatableAddressFields
import mozilla.components.concept.storage.UpdatableCreditCardFields
import mozilla.components.lib.dataprotect.SecureAbove22Preferences
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@ExperimentalCoroutinesApi // for runTest
@RunWith(AndroidJUnit4::class)
class AutofillCreditCardsAddressesStorageTest {

    private lateinit var storage: AutofillCreditCardsAddressesStorage
    private lateinit var securePrefs: SecureAbove22Preferences

    @Before
    fun setup() {
        // forceInsecure is set in the tests because a keystore wouldn't be configured in the test environment.
        securePrefs = SecureAbove22Preferences(testContext, "autofill", forceInsecure = true)
        storage = AutofillCreditCardsAddressesStorage(testContext, lazy { securePrefs })
    }

    @After
    fun cleanup() {
        storage.close()
    }

    @Test
    fun `add credit card`() = runTest {
        val plaintextNumber = CreditCardNumber.Plaintext("4111111111111111")
        val creditCardFields = NewCreditCardFields(
            billingName = "Jon Doe",
            plaintextCardNumber = plaintextNumber,
            cardNumberLast4 = "1111",
            expiryMonth = 12,
            expiryYear = 2028,
            cardType = "amex",
        )
        val creditCard = storage.addCreditCard(creditCardFields)

        assertNotNull(creditCard)

        assertEquals(creditCardFields.billingName, creditCard.billingName)
        assertEquals(plaintextNumber, storage.crypto.decrypt(storage.crypto.getOrGenerateKey(), creditCard.encryptedCardNumber))
        assertEquals(creditCardFields.cardNumberLast4, creditCard.cardNumberLast4)
        assertEquals(creditCardFields.expiryMonth, creditCard.expiryMonth)
        assertEquals(creditCardFields.expiryYear, creditCard.expiryYear)
        assertEquals(creditCardFields.cardType, creditCard.cardType)
        assertEquals(
            CreditCard.ellipsesStart +
                CreditCard.ellipsis + CreditCard.ellipsis + CreditCard.ellipsis + CreditCard.ellipsis +
                creditCardFields.cardNumberLast4 +
                CreditCard.ellipsesEnd,
            creditCard.obfuscatedCardNumber,
        )
    }

    @Test
    fun `get credit card`() = runTest {
        val plaintextNumber = CreditCardNumber.Plaintext("5500000000000004")
        val creditCardFields = NewCreditCardFields(
            billingName = "Jon Doe",
            plaintextCardNumber = plaintextNumber,
            cardNumberLast4 = "0004",
            expiryMonth = 12,
            expiryYear = 2028,
            cardType = "amex",
        )
        val creditCard = storage.addCreditCard(creditCardFields)

        assertEquals(creditCard, storage.getCreditCard(creditCard.guid))
    }

    @Test
    fun `GIVEN a non-existent credit card guid WHEN getCreditCard is called THEN null is returned`() = runTest {
        assertNull(storage.getCreditCard("guid"))
    }

    @Test
    fun `get all credit cards`() = runTest {
        val plaintextNumber1 = CreditCardNumber.Plaintext("5500000000000004")
        val creditCardFields1 = NewCreditCardFields(
            billingName = "Jane Fields",
            plaintextCardNumber = plaintextNumber1,
            cardNumberLast4 = "0004",
            expiryMonth = 12,
            expiryYear = 2028,
            cardType = "mastercard",
        )
        val plaintextNumber2 = CreditCardNumber.Plaintext("4111111111111111")
        val creditCardFields2 = NewCreditCardFields(
            billingName = "Banana Apple",
            plaintextCardNumber = plaintextNumber2,
            cardNumberLast4 = "1111",
            expiryMonth = 1,
            expiryYear = 2030,
            cardType = "visa",
        )
        val plaintextNumber3 = CreditCardNumber.Plaintext("340000000000009")
        val creditCardFields3 = NewCreditCardFields(
            billingName = "Pineapple Orange",
            plaintextCardNumber = plaintextNumber3,
            cardNumberLast4 = "0009",
            expiryMonth = 2,
            expiryYear = 2028,
            cardType = "amex",
        )
        val creditCard1 = storage.addCreditCard(creditCardFields1)
        val creditCard2 = storage.addCreditCard(creditCardFields2)
        val creditCard3 = storage.addCreditCard(creditCardFields3)

        val creditCards = storage.getAllCreditCards()
        val key = storage.crypto.getOrGenerateKey()

        val savedCreditCard1 = creditCards.find { it == creditCard1 }
        assertNotNull(savedCreditCard1)
        val savedCreditCard2 = creditCards.find { it == creditCard2 }
        assertNotNull(savedCreditCard2)
        val savedCreditCard3 = creditCards.find { it == creditCard3 }
        assertNotNull(savedCreditCard3)

        assertEquals(plaintextNumber1, storage.crypto.decrypt(key, savedCreditCard1!!.encryptedCardNumber))
        assertEquals(plaintextNumber2, storage.crypto.decrypt(key, savedCreditCard2!!.encryptedCardNumber))
        assertEquals(plaintextNumber3, storage.crypto.decrypt(key, savedCreditCard3!!.encryptedCardNumber))
    }

    @Test
    fun `update credit card`() = runTest {
        val creditCardFields = NewCreditCardFields(
            billingName = "Jon Doe",
            plaintextCardNumber = CreditCardNumber.Plaintext("4111111111111111"),
            cardNumberLast4 = "1111",
            expiryMonth = 12,
            expiryYear = 2028,
            cardType = "visa",
        )

        var creditCard = storage.addCreditCard(creditCardFields)

        // Change every field
        var newCreditCardFields = UpdatableCreditCardFields(
            billingName = "Jane Fields",
            cardNumber = CreditCardNumber.Plaintext("30000000000004"),
            cardNumberLast4 = "0004",
            expiryMonth = 12,
            expiryYear = 2038,
            cardType = "diners",
        )

        storage.updateCreditCard(creditCard.guid, newCreditCardFields)

        creditCard = storage.getCreditCard(creditCard.guid)!!

        val key = storage.crypto.getOrGenerateKey()

        assertEquals(newCreditCardFields.billingName, creditCard.billingName)
        assertEquals(newCreditCardFields.cardNumber, storage.crypto.decrypt(key, creditCard.encryptedCardNumber))
        assertEquals(newCreditCardFields.cardNumberLast4, creditCard.cardNumberLast4)
        assertEquals(newCreditCardFields.expiryMonth, creditCard.expiryMonth)
        assertEquals(newCreditCardFields.expiryYear, creditCard.expiryYear)
        assertEquals(newCreditCardFields.cardType, creditCard.cardType)

        // Change the name only.
        newCreditCardFields = UpdatableCreditCardFields(
            billingName = "Bob Jones",
            cardNumber = creditCard.encryptedCardNumber,
            cardNumberLast4 = "0004",
            expiryMonth = 12,
            expiryYear = 2038,
            cardType = "diners",
        )

        storage.updateCreditCard(creditCard.guid, newCreditCardFields)

        creditCard = storage.getCreditCard(creditCard.guid)!!

        assertEquals(newCreditCardFields.billingName, creditCard.billingName)
        assertEquals(newCreditCardFields.cardNumber, creditCard.encryptedCardNumber)
        assertEquals(newCreditCardFields.cardNumberLast4, creditCard.cardNumberLast4)
        assertEquals(newCreditCardFields.expiryMonth, creditCard.expiryMonth)
        assertEquals(newCreditCardFields.expiryYear, creditCard.expiryYear)
        assertEquals(newCreditCardFields.cardType, creditCard.cardType)
    }

    @Test
    fun `delete credit card`() = runTest {
        val creditCardFields = NewCreditCardFields(
            billingName = "Jon Doe",
            plaintextCardNumber = CreditCardNumber.Plaintext("30000000000004"),
            cardNumberLast4 = "0004",
            expiryMonth = 12,
            expiryYear = 2028,
            cardType = "diners",
        )

        val creditCard = storage.addCreditCard(creditCardFields)
        assertNotNull(storage.getCreditCard(creditCard.guid))

        val isDeleteSuccessful = storage.deleteCreditCard(creditCard.guid)

        assertTrue(isDeleteSuccessful)
        assertNull(storage.getCreditCard(creditCard.guid))
    }

    @Test
    fun `add address`() = runTest {
        val addressFields = UpdatableAddressFields(
            givenName = "John",
            additionalName = "",
            familyName = "Smith",
            organization = "Mozilla",
            streetAddress = "123 Sesame Street",
            addressLevel3 = "",
            addressLevel2 = "",
            addressLevel1 = "",
            postalCode = "90210",
            country = "US",
            tel = "+1 519 555-5555",
            email = "foo@bar.com",
        )
        val address = storage.addAddress(addressFields)

        assertNotNull(address)

        assertEquals(addressFields.givenName, address.givenName)
        assertEquals(addressFields.additionalName, address.additionalName)
        assertEquals(addressFields.familyName, address.familyName)
        assertEquals(addressFields.organization, address.organization)
        assertEquals(addressFields.streetAddress, address.streetAddress)
        assertEquals(addressFields.addressLevel3, address.addressLevel3)
        assertEquals(addressFields.addressLevel2, address.addressLevel2)
        assertEquals(addressFields.addressLevel1, address.addressLevel1)
        assertEquals(addressFields.postalCode, address.postalCode)
        assertEquals(addressFields.country, address.country)
        assertEquals(addressFields.tel, address.tel)
        assertEquals(addressFields.email, address.email)
    }

    @Test
    fun `get address`() = runTest {
        val addressFields = UpdatableAddressFields(
            givenName = "John",
            additionalName = "",
            familyName = "Smith",
            organization = "Mozilla",
            streetAddress = "123 Sesame Street",
            addressLevel3 = "",
            addressLevel2 = "",
            addressLevel1 = "",
            postalCode = "90210",
            country = "US",
            tel = "+1 519 555-5555",
            email = "foo@bar.com",
        )
        val address = storage.addAddress(addressFields)

        assertEquals(address, storage.getAddress(address.guid))
    }

    @Test
    fun `GIVEN a non-existent address guid WHEN getAddress is called THEN null is returned`() = runTest {
        assertNull(storage.getAddress("guid"))
    }

    @Test
    fun `get all addresses`() = runTest {
        val addressFields1 = UpdatableAddressFields(
            givenName = "John",
            additionalName = "",
            familyName = "Smith",
            organization = "Mozilla",
            streetAddress = "123 Sesame Street",
            addressLevel3 = "",
            addressLevel2 = "",
            addressLevel1 = "",
            postalCode = "90210",
            country = "US",
            tel = "+1 519 555-5555",
            email = "foo@bar.com",
        )
        val addressFields2 = UpdatableAddressFields(
            givenName = "Mary",
            additionalName = "",
            familyName = "Sue",
            organization = "",
            streetAddress = "1 New St",
            addressLevel3 = "",
            addressLevel2 = "York",
            addressLevel1 = "SC",
            postalCode = "29745",
            country = "US",
            tel = "+19871234567",
            email = "mary@example.com",
        )
        val addressFields3 = UpdatableAddressFields(
            givenName = "Timothy",
            additionalName = "João",
            familyName = "Berners-Lee",
            organization = "World Wide Web Consortium",
            streetAddress = "Rua Adalberto Pajuaba, 404",
            addressLevel3 = "Campos Elísios",
            addressLevel2 = "Ribeirão Preto",
            addressLevel1 = "SP",
            postalCode = "14055-220",
            country = "BR",
            tel = "+0318522222222",
            email = "timbr@example.org",
        )
        val address1 = storage.addAddress(addressFields1)
        val address2 = storage.addAddress(addressFields2)
        val address3 = storage.addAddress(addressFields3)

        val addresses = storage.getAllAddresses()

        val savedAddress1 = addresses.find { it == address1 }
        assertNotNull(savedAddress1)
        val savedAddress2 = addresses.find { it == address2 }
        assertNotNull(savedAddress2)
        val savedAddress3 = addresses.find { it == address3 }
        assertNotNull(savedAddress3)
    }

    @Test
    fun `update address`() = runTest {
        val addressFields = UpdatableAddressFields(
            givenName = "John",
            additionalName = "",
            familyName = "Smith",
            organization = "Mozilla",
            streetAddress = "123 Sesame Street",
            addressLevel3 = "",
            addressLevel2 = "",
            addressLevel1 = "",
            postalCode = "90210",
            country = "US",
            tel = "+1 519 555-5555",
            email = "foo@bar.com",
        )

        var address = storage.addAddress(addressFields)

        val newAddressFields = UpdatableAddressFields(
            givenName = "Mary",
            additionalName = "",
            familyName = "Sue",
            organization = "",
            streetAddress = "1 New St",
            addressLevel3 = "",
            addressLevel2 = "York",
            addressLevel1 = "SC",
            postalCode = "29745",
            country = "US",
            tel = "+19871234567",
            email = "mary@example.com",
        )

        storage.updateAddress(address.guid, newAddressFields)

        address = storage.getAddress(address.guid)!!

        assertEquals(newAddressFields.givenName, address.givenName)
        assertEquals(newAddressFields.additionalName, address.additionalName)
        assertEquals(newAddressFields.familyName, address.familyName)
        assertEquals(newAddressFields.organization, address.organization)
        assertEquals(newAddressFields.streetAddress, address.streetAddress)
        assertEquals(newAddressFields.addressLevel3, address.addressLevel3)
        assertEquals(newAddressFields.addressLevel2, address.addressLevel2)
        assertEquals(newAddressFields.addressLevel1, address.addressLevel1)
        assertEquals(newAddressFields.postalCode, address.postalCode)
        assertEquals(newAddressFields.country, address.country)
        assertEquals(newAddressFields.tel, address.tel)
        assertEquals(newAddressFields.email, address.email)
    }

    @Test
    fun `delete address`() = runTest {
        val addressFields = UpdatableAddressFields(
            givenName = "John",
            additionalName = "",
            familyName = "Smith",
            organization = "Mozilla",
            streetAddress = "123 Sesame Street",
            addressLevel3 = "",
            addressLevel2 = "",
            addressLevel1 = "",
            postalCode = "90210",
            country = "US",
            tel = "+1 519 555-5555",
            email = "foo@bar.com",
        )

        val address = storage.addAddress(addressFields)
        val savedAddress = storage.getAddress(address.guid)
        assertEquals(address, savedAddress)

        val isDeleteSuccessful = storage.deleteAddress(address.guid)
        assertTrue(isDeleteSuccessful)
        assertNull(storage.getAddress(address.guid))
    }

    @Test
    fun `WHEN warmUp method is called THEN the database connection is established`(): Unit = runTest {
        val storageSpy = spy(storage)
        storageSpy.warmUp()

        verify(storageSpy).conn
    }
}
