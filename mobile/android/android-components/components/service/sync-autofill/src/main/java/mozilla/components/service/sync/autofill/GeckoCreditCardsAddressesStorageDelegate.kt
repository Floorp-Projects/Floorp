/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.sync.autofill

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.concept.storage.Address
import mozilla.components.concept.storage.CreditCard
import mozilla.components.concept.storage.CreditCardEntry
import mozilla.components.concept.storage.CreditCardNumber
import mozilla.components.concept.storage.CreditCardValidationDelegate
import mozilla.components.concept.storage.CreditCardsAddressesStorage
import mozilla.components.concept.storage.CreditCardsAddressesStorageDelegate
import mozilla.components.concept.storage.ManagedKey
import mozilla.components.concept.storage.NewCreditCardFields
import mozilla.components.concept.storage.UpdatableCreditCardFields
import mozilla.components.support.ktx.kotlin.last4Digits

/**
 * [CreditCardsAddressesStorageDelegate] implementation.
 *
 * @param storage The [CreditCardsAddressesStorage] used for looking up addresses and credit cards to autofill.
 * @param scope [CoroutineScope] for long running operations. Defaults to using the [Dispatchers.IO].
 * @param isCreditCardAutofillEnabled callback allowing to limit [storage] operations if autofill is disabled.
 * @param validationDelegate The [DefaultCreditCardValidationDelegate] used to check if a credit card
 * can be saved in [storage] and returns information about why it can or cannot
 */
class GeckoCreditCardsAddressesStorageDelegate(
    private val storage: Lazy<CreditCardsAddressesStorage>,
    private val scope: CoroutineScope = CoroutineScope(Dispatchers.IO),
    private val validationDelegate: DefaultCreditCardValidationDelegate = DefaultCreditCardValidationDelegate(storage),
    private val isCreditCardAutofillEnabled: () -> Boolean = { false },
    private val isAddressAutofillEnabled: () -> Boolean = { false },
) : CreditCardsAddressesStorageDelegate {

    override suspend fun getOrGenerateKey(): ManagedKey {
        val crypto = storage.value.getCreditCardCrypto()
        return crypto.getOrGenerateKey()
    }

    override suspend fun decrypt(
        key: ManagedKey,
        encryptedCardNumber: CreditCardNumber.Encrypted,
    ): CreditCardNumber.Plaintext? {
        val crypto = storage.value.getCreditCardCrypto()
        return crypto.decrypt(key, encryptedCardNumber)
    }

    override suspend fun onAddressesFetch(): List<Address> = withContext(scope.coroutineContext) {
        if (!isAddressAutofillEnabled()) {
            emptyList()
        } else {
            storage.value.getAllAddresses()
        }
    }

    override suspend fun onAddressSave(address: Address) {
        TODO("Not yet implemented")
    }

    override suspend fun onCreditCardsFetch(): List<CreditCard> =
        withContext(scope.coroutineContext) {
            if (!isCreditCardAutofillEnabled()) {
                emptyList()
            } else {
                storage.value.getAllCreditCards()
            }
        }

    override suspend fun onCreditCardSave(creditCard: CreditCardEntry) {
        scope.launch {
            when (val result = validationDelegate.shouldCreateOrUpdate(creditCard)) {
                is CreditCardValidationDelegate.Result.CanBeCreated -> {
                    storage.value.addCreditCard(
                        NewCreditCardFields(
                            billingName = creditCard.name,
                            plaintextCardNumber = CreditCardNumber.Plaintext(creditCard.number),
                            cardNumberLast4 = creditCard.number.last4Digits(),
                            expiryMonth = creditCard.expiryMonth.toLong(),
                            expiryYear = creditCard.expiryYear.toLong(),
                            cardType = creditCard.cardType,
                        ),
                    )
                }
                is CreditCardValidationDelegate.Result.CanBeUpdated -> {
                    storage.value.updateCreditCard(
                        guid = result.foundCreditCard.guid,
                        creditCardFields = UpdatableCreditCardFields(
                            billingName = creditCard.name,
                            cardNumber = CreditCardNumber.Plaintext(creditCard.number),
                            cardNumberLast4 = creditCard.number.last4Digits(),
                            expiryMonth = creditCard.expiryMonth.toLong(),
                            expiryYear = creditCard.expiryYear.toLong(),
                            cardType = creditCard.cardType,
                        ),
                    )
                }
            }
        }
    }
}
