/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.sync.autofill

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import mozilla.components.concept.storage.UpdatableAddressFields
import mozilla.components.concept.storage.UpdatableCreditCardFields
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class AutofillCreditCardsAddressesStorageTest {
    private lateinit var storage: AutofillCreditCardsAddressesStorage

    @Before
    fun setup() = runBlocking {
        storage = AutofillCreditCardsAddressesStorage(testContext)
    }

    @After
    fun cleanup() = runBlocking {
        storage.close()
    }

    @Test
    fun `addCreditCard`() = runBlocking {
        val creditCardFields = UpdatableCreditCardFields(
            billingName = "Jon Doe",
            cardNumber = "4111111111111111",
            expiryMonth = 12,
            expiryYear = 2028,
            cardType = "amex"
        )
        val creditCard = storage.addCreditCard(creditCardFields)

        assertNotNull(creditCard)

        assertEquals(creditCardFields.billingName, creditCard.billingName)
        assertEquals(creditCardFields.cardNumber, creditCard.cardNumber)
        assertEquals(creditCardFields.expiryMonth, creditCard.expiryMonth)
        assertEquals(creditCardFields.expiryYear, creditCard.expiryYear)
        assertEquals(creditCardFields.cardType, creditCard.cardType)
    }

    @Test
    fun `getCreditCard`() = runBlocking {
        val creditCardFields = UpdatableCreditCardFields(
            billingName = "Jon Doe",
            cardNumber = "4111111111111111",
            expiryMonth = 12,
            expiryYear = 2028,
            cardType = "amex"
        )
        val creditCard = storage.addCreditCard(creditCardFields)

        assertEquals(creditCard, storage.getCreditCard(creditCard.guid))
    }

    @Test
    fun `getAllCreditCards`() = runBlocking {
        var creditCards = storage.getAllCreditCards()
        assertEquals(0, creditCards.size)

        val creditCardFields1 = UpdatableCreditCardFields(
            billingName = "Jane Fields",
            cardNumber = "4111111111111111",
            expiryMonth = 12,
            expiryYear = 2028,
            cardType = "amex"
        )
        val creditCardFields2 = UpdatableCreditCardFields(
            billingName = "Banana Apple",
            cardNumber = "4111111111111112",
            expiryMonth = 1,
            expiryYear = 2030,
            cardType = "discover"
        )
        val creditCardFields3 = UpdatableCreditCardFields(
            billingName = "Pineapple Orange",
            cardNumber = "4111111111111113",
            expiryMonth = 2,
            expiryYear = 2028,
            cardType = "visa"
        )
        val creditCard1 = storage.addCreditCard(creditCardFields1)
        val creditCard2 = storage.addCreditCard(creditCardFields2)
        val creditCard3 = storage.addCreditCard(creditCardFields3)

        creditCards = storage.getAllCreditCards()

        assertEquals(3, creditCards.size)
        assertEquals(creditCard1, creditCards[0])
        assertEquals(creditCard2, creditCards[1])
        assertEquals(creditCard3, creditCards[2])
    }

    @Test
    fun `updateCreditCard`() = runBlocking {
        val creditCardFields = UpdatableCreditCardFields(
            billingName = "Jon Doe",
            cardNumber = "4111111111111111",
            expiryMonth = 12,
            expiryYear = 2028,
            cardType = "amex"
        )

        var creditCard = storage.addCreditCard(creditCardFields)

        val newCreditCardFields = UpdatableCreditCardFields(
            billingName = "Jane Fields",
            cardNumber = "4111111111111112",
            expiryMonth = 12,
            expiryYear = 2038,
            cardType = "visa"
        )

        storage.updateCreditCard(creditCard.guid, newCreditCardFields)

        creditCard = storage.getCreditCard(creditCard.guid)

        assertEquals(newCreditCardFields.billingName, creditCard.billingName)
        assertEquals(newCreditCardFields.cardNumber, creditCard.cardNumber)
        assertEquals(newCreditCardFields.expiryMonth, creditCard.expiryMonth)
        assertEquals(newCreditCardFields.expiryYear, creditCard.expiryYear)
        assertEquals(newCreditCardFields.cardType, creditCard.cardType)
    }

    @Test
    fun `deleteCreditCard`() = runBlocking {
        val creditCardFields = UpdatableCreditCardFields(
            billingName = "Jon Doe",
            cardNumber = "4111111111111111",
            expiryMonth = 12,
            expiryYear = 2028,
            cardType = "amex"
        )

        val creditCard = storage.addCreditCard(creditCardFields)
        val creditCards = storage.getAllCreditCards()

        assertEquals(1, creditCards.size)
        assertEquals(creditCard, creditCards[0])

        val isDeleteSuccessful = storage.deleteCreditCard(creditCard.guid)

        assertTrue(isDeleteSuccessful)
        assertEquals(0, storage.getAllCreditCards().size)
    }

    @Test
    fun `addAddress`() = runBlocking {
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
            email = "foo@bar.com"
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
    fun `getAddress`() = runBlocking {
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
            email = "foo@bar.com"
        )
        val address = storage.addAddress(addressFields)

        assertEquals(address, storage.getAddress(address.guid))
    }

    @Test
    fun `getAllAddresses`() = runBlocking {
        var addresses = storage.getAllAddresses()
        assertEquals(0, addresses.size)

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
            email = "foo@bar.com"
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
            email = "mary@example.com"
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
            email = "timbr@example.org"
        )
        val address1 = storage.addAddress(addressFields1)
        val address2 = storage.addAddress(addressFields2)
        val address3 = storage.addAddress(addressFields3)

        addresses = storage.getAllAddresses()

        assertEquals(3, addresses.size)
        assertEquals(address1, addresses[0])
        assertEquals(address2, addresses[1])
        assertEquals(address3, addresses[2])
    }

    @Test
    fun `updateAddress`() = runBlocking {
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
            email = "foo@bar.com"
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
            email = "mary@example.com"
        )

        storage.updateAddress(address.guid, newAddressFields)

        address = storage.getAddress(address.guid)

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
    fun `deleteAddress`() = runBlocking {
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
            email = "foo@bar.com"
        )

        val address = storage.addAddress(addressFields)
        val addresses = storage.getAllAddresses()

        assertEquals(1, addresses.size)
        assertEquals(address, addresses[0])

        val isDeleteSuccessful = storage.deleteAddress(address.guid)

        assertTrue(isDeleteSuccessful)
        assertEquals(0, storage.getAllAddresses().size)
    }
}
