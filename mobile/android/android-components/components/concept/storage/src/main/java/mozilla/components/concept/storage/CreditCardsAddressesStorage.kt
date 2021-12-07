/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.storage

import android.annotation.SuppressLint
import android.os.Parcelable
import kotlinx.coroutines.Deferred
import kotlinx.parcelize.Parcelize

/**
 * An interface which defines read/write methods for credit card and address data.
 */
interface CreditCardsAddressesStorage {

    /**
     * Inserts the provided credit card into the database, and returns
     * the newly added [CreditCard].
     *
     * @param creditCardFields A [NewCreditCardFields] record to add.
     * @return [CreditCard] for the added credit card.
     */
    suspend fun addCreditCard(creditCardFields: NewCreditCardFields): CreditCard

    /**
     * Updates the fields in the provided credit card.
     *
     * @param guid Unique identifier for the desired credit card.
     * @param creditCardFields A set of credit card fields, wrapped in [UpdatableCreditCardFields], to update.
     */
    suspend fun updateCreditCard(guid: String, creditCardFields: UpdatableCreditCardFields)

    /**
     * Retrieves the credit card from the underlying storage layer by its unique identifier.
     *
     * @param guid Unique identifier for the desired credit card.
     * @return [CreditCard] record if it exists or null otherwise.
     */
    suspend fun getCreditCard(guid: String): CreditCard?

    /**
     * Retrieves a list of all the credit cards.
     *
     * @return A list of all [CreditCard].
     */
    suspend fun getAllCreditCards(): List<CreditCard>

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
     * @return [Address] record if it exists or null otherwise.
     */
    suspend fun getAddress(guid: String): Address?

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

    /**
     * Returns an instance of [CreditCardCrypto] that knows how to encrypt and decrypt credit card
     * numbers.
     *
     * @return [CreditCardCrypto] instance.
     */
    fun getCreditCardCrypto(): CreditCardCrypto
}

/**
 * An interface that defines methods for encrypting and decrypting a credit card number.
 */
interface CreditCardCrypto : KeyProvider {

    /**
     * Encrypt a [CreditCardNumber.Plaintext] using the provided key. A `null` result means a
     * bad key was provided. In that case caller should obtain a new key and try again.
     *
     * @param key The encryption key to encrypt the plaintext credit card number.
     * @param plaintextCardNumber A plaintext credit card number to be encrypted.
     * @return An encrypted credit card number or `null` if a bad [key] was provided.
     */
    fun encrypt(
        key: ManagedKey,
        plaintextCardNumber: CreditCardNumber.Plaintext
    ): CreditCardNumber.Encrypted?

    /**
     * Decrypt a [CreditCardNumber.Encrypted] using the provided key. A `null` result means a
     * bad key was provided. In that case caller should obtain a new key and try again.
     *
     * @param key The encryption key to decrypt the decrypt credit card number.
     * @param encryptedCardNumber An encrypted credit card number to be decrypted.
     * @return A plaintext, non-encrypted credit card number or `null` if a bad [key] was provided.
     */
    fun decrypt(
        key: ManagedKey,
        encryptedCardNumber: CreditCardNumber.Encrypted
    ): CreditCardNumber.Plaintext?
}

/**
 * A credit card number. This structure exists to provide better typing at the API surface.
 *
 * @property number Either a plaintext or a ciphertext of the credit card number, depending on the subtype.
 */
sealed class CreditCardNumber(val number: String) {
    /**
     * An encrypted credit card number.
     */
    @SuppressLint("ParcelCreator")
    @Parcelize
    data class Encrypted(private val data: String) : CreditCardNumber(data), Parcelable

    /**
     * A plaintext, non-encrypted credit card number.
     */
    data class Plaintext(private val data: String) : CreditCardNumber(data)
}

/**
 * Information about a credit card.
 *
 * @property guid The unique identifier for this credit card.
 * @property billingName The credit card billing name.
 * @property encryptedCardNumber The encrypted credit card number.
 * @property cardNumberLast4 The last 4 digits of the credit card number.
 * @property expiryMonth The credit card expiry month.
 * @property expiryYear The credit card expiry year.
 * @property cardType The credit card network ID.
 * @property timeCreated Time of creation in milliseconds from the unix epoch.
 * @property timeLastUsed Time of last use in milliseconds from the unix epoch.
 * @property timeLastModified Time of last modified in milliseconds from the unix epoch.
 * @property timesUsed Number of times the credit card was used.
 */
@SuppressLint("ParcelCreator")
@Parcelize
data class CreditCard(
    val guid: String,
    val billingName: String,
    val encryptedCardNumber: CreditCardNumber.Encrypted,
    val cardNumberLast4: String,
    val expiryMonth: Long,
    val expiryYear: Long,
    val cardType: String,
    val timeCreated: Long,
    val timeLastUsed: Long?,
    val timeLastModified: Long,
    val timesUsed: Long
) : Parcelable {
    val obfuscatedCardNumber: String
        get() = ellipsesStart +
            ellipsis + ellipsis + ellipsis + ellipsis +
            cardNumberLast4 +
            ellipsesEnd

    companion object {
        // Left-To-Right Embedding (LTE) mark
        const val ellipsesStart = "\u202A"

        // One dot ellipsis
        const val ellipsis = "\u2022\u2060\u2006\u2060"

        // Pop Directional Formatting (PDF) mark
        const val ellipsesEnd = "\u202C"
    }
}

/**
 * Information about a new credit card.
 * Use this when creating a credit card via [CreditCardsAddressesStorage.addCreditCard].
 *
 * @property billingName The credit card billing name.
 * @property plaintextCardNumber A plaintext credit card number.
 * @property cardNumberLast4 The last 4 digits of the credit card number.
 * @property expiryMonth The credit card expiry month.
 * @property expiryYear The credit card expiry year.
 * @property cardType The credit card network ID.
 */
data class NewCreditCardFields(
    val billingName: String,
    val plaintextCardNumber: CreditCardNumber.Plaintext,
    val cardNumberLast4: String,
    val expiryMonth: Long,
    val expiryYear: Long,
    val cardType: String
)

/**
 * Information about a new credit card.
 * Use this when creating a credit card via [CreditCardsAddressesStorage.updateAddress].
 *
 * @property billingName The credit card billing name.
 * @property cardNumber A [CreditCardNumber] that is either encrypted or plaintext. Passing in plaintext
 * version will update the stored credit card number.
 * @property cardNumberLast4 The last 4 digits of the credit card number.
 * @property expiryMonth The credit card expiry month.
 * @property expiryYear The credit card expiry year.
 * @property cardType The credit card network ID.
 */
data class UpdatableCreditCardFields(
    val billingName: String,
    val cardNumber: CreditCardNumber,
    val cardNumberLast4: String,
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
     * Decrypt a [CreditCardNumber.Encrypted] into its plaintext equivalent or `null` if
     * it fails to decrypt.
     *
     * @param encryptedCardNumber An encrypted credit card number to be decrypted.
     * @return A plaintext, non-encrypted credit card number.
     */
    suspend fun decrypt(encryptedCardNumber: CreditCardNumber.Encrypted): CreditCardNumber.Plaintext?

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
