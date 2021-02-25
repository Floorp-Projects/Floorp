/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.storage

import kotlinx.coroutines.Deferred

/**
 * An interface which defines read/write methods for credit card and address data.
 */
@SuppressWarnings("TooManyFunctions")
interface CreditCardsAddressesStorage {

    /**
     * Inserts the provided credit card into the database, and returns
     * the newly added [CreditCard].
     *
     * @param creditCardFields A [UpdatableCreditCardFields] record to add.
     * @return [CreditCard] for the added credit card.
     */
    suspend fun addCreditCard(creditCardFields: UpdatableCreditCardFields): CreditCard

    /**
     * Retrieves the credit card from the underlying storage layer by its unique identifier.
     *
     * @param guid Unique identifier for the desired credit card.
     * @return [CreditCard] record.
     */
    suspend fun getCreditCard(guid: String): CreditCard

    /**
     * Retrieves a list of all the credit cards.
     *
     * @return A list of all [CreditCard].
     */
    suspend fun getAllCreditCards(): List<CreditCard>

    /**
     * Updates the fields in the provided credit card.
     *
     * @param guid Unique identifier for the desired credit card.
     * @param creditCardFields The credit card fields to update.
     */
    suspend fun updateCreditCard(guid: String, creditCardFields: UpdatableCreditCardFields)

    /**
     * Deletes the credit card with the given [guid].
     *
     * @return True if the deletion did anything, false otherwise.
     */
    suspend fun deleteCreditCard(guid: String): Boolean

    /**
     * Marks the credit card with the given [guid] as `in-use`.
     *
     * @param guid Unique identifier for the desired credit card.
     */
    suspend fun touchCreditCard(guid: String)

    /**
     * Inserts the provided address into the database, and returns
     * the newly added [Address].
     *
     * @param addressFields A [UpdatableAddressFields] record to add.
     * @return [Address] for the added address.
     */
    suspend fun addAddress(addressFields: UpdatableAddressFields): Address

    /**
     * Retrieves the address from the underlying storage layer by its unique identifier.
     *
     * @param guid Unique identifier for the desired address.
     * @return [Address] record.
     */
    suspend fun getAddress(guid: String): Address

    /**
     * Retrieves a list of all the addresses.
     *
     * @return A list of all [Address].
     */
    suspend fun getAllAddresses(): List<Address>

    /**
     * Updates the fields in the provided address.
     *
     * @param guid Unique identifier for the desired address.
     * @param address The address fields to update.
     */
    suspend fun updateAddress(guid: String, address: UpdatableAddressFields)

    /**
     * Delete the address with the given [guid].
     *
     * @return True if the deletion did anything, false otherwise.
     */
    suspend fun deleteAddress(guid: String): Boolean

    /**
     * Marks the address with the given [guid] as `in-use`.
     *
     * @param guid Unique identifier for the desired address.
     */
    suspend fun touchAddress(guid: String)
}

/**
 * Information about a credit card.
 *
 * @property guid The unique identifier for this credit card.
 * @property billingName The credit card billing name.
 * @property cardNumber The credit card number.
 * @property expiryMonth The credit card expiry month.
 * @property expiryYear The credit card expiry year.
 * @property cardType The credit card network ID.
 * @property timeCreated Time of creation in milliseconds from the unix epoch.
 * @property timeLastUsed Time of last use in milliseconds from the unix epoch.
 * @property timeLastModified Time of last modified in milliseconds from the unix epoch.
 * @property timesUsed Number of times the credit card was used.
 */
data class CreditCard(
    val guid: String,
    val billingName: String,
    val cardNumber: String,
    val expiryMonth: Long,
    val expiryYear: Long,
    val cardType: String,
    val timeCreated: Long,
    val timeLastUsed: Long?,
    val timeLastModified: Long,
    val timesUsed: Long
)

/**
 * Information about a new credit card. This is what you pass to create or update a credit card.
 *
 * @property billingName The credit card billing name.
 * @property cardNumber The credit card number.
 * @property expiryMonth The credit card expiry month.
 * @property expiryYear The credit card expiry year.
 * @property cardType The credit card network ID.
 */
data class UpdatableCreditCardFields(
    val billingName: String,
    val cardNumber: String,
    val expiryMonth: Long,
    val expiryYear: Long,
    val cardType: String
)

/**
 * Information about a address.
 *
 * @property guid The unique identifier for this address.
 * @property givenName First name.
 * @property additionalName Middle name.
 * @property familyName Last name.
 * @property organization Organization.
 * @property streetAddress Street address.
 * @property addressLevel3 Sublocality (Suburb) name type.
 * @property addressLevel2 Locality (City/Town) name type.
 * @property addressLevel1 Province/State name type.
 * @property postalCode Postal code.
 * @property country Country.
 * @property tel Telephone number.
 * @property email E-mail address.
 * @property timeCreated Time of creation in milliseconds from the unix epoch.
 * @property timeLastUsed Time of last use in milliseconds from the unix epoch.
 * @property timeLastModified Time of last modified in milliseconds from the unix epoch.
 * @property timesUsed Number of times the address was used.
 */
data class Address(
    val guid: String,
    val givenName: String,
    val additionalName: String,
    val familyName: String,
    val organization: String,
    val streetAddress: String,
    val addressLevel3: String,
    val addressLevel2: String,
    val addressLevel1: String,
    val postalCode: String,
    val country: String,
    val tel: String,
    val email: String,
    val timeCreated: Long,
    val timeLastUsed: Long?,
    val timeLastModified: Long,
    val timesUsed: Long
)

/**
 * Information about a new address. This is what you pass to create or update an address.
 *
 * @property givenName First name.
 * @property additionalName Middle name.
 * @property familyName Last name.
 * @property organization Organization.
 * @property streetAddress Street address.
 * @property addressLevel3 Sublocality (Suburb) name type.
 * @property addressLevel2 Locality (City/Town) name type.
 * @property addressLevel1 Province/State name type.
 * @property postalCode Postal code.
 * @property country Country.
 * @property tel Telephone number.
 * @property email E-mail address.
 */
data class UpdatableAddressFields(
    val givenName: String,
    val additionalName: String,
    val familyName: String,
    val organization: String,
    val streetAddress: String,
    val addressLevel3: String,
    val addressLevel2: String,
    val addressLevel1: String,
    val postalCode: String,
    val country: String,
    val tel: String,
    val email: String
)

/**
 * Used to handle [Address] and [CreditCard] storage so that the underlying engine doesn't have to.
 * An instance of this should be attached to the Gecko runtime in order to be used.
 */
interface CreditCardsAddressesStorageDelegate {

    /**
     * Returns all stored addresses. This is called when the engine believes an address field
     * should be autofilled.
     */
    fun onAddressesFetch(): Deferred<List<Address>>

    /**
     * Saves the given address to storage.
     */
    fun onAddressSave(address: Address)

    /**
     * Returns all stored credit cards. This is called when the engine believes a credit card
     * field should be autofilled.
     */
    fun onCreditCardsFetch(): Deferred<List<CreditCard>>

    /**
     * Saves the given credit card to storage.
     */
    fun onCreditCardSave(creditCard: CreditCard)
}
