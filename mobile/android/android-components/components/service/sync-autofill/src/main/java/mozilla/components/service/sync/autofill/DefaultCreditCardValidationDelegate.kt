/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.sync.autofill

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.concept.storage.CreditCard
import mozilla.components.concept.storage.CreditCardEntry
import mozilla.components.concept.storage.CreditCardValidationDelegate
import mozilla.components.concept.storage.CreditCardValidationDelegate.Result
import mozilla.components.concept.storage.CreditCardsAddressesStorage

/**
 * A delegate that will check against the [CreditCardsAddressesStorage] to determine if a given
 * [CreditCard] can be persisted and returns information about why it can or cannot.
 *
 * @param storage An instance of [CreditCardsAddressesStorage].
 */
class DefaultCreditCardValidationDelegate(
    private val storage: Lazy<CreditCardsAddressesStorage>,
) : CreditCardValidationDelegate {

    private val coroutineContext by lazy { Dispatchers.IO }

    override suspend fun shouldCreateOrUpdate(creditCard: CreditCardEntry): Result =
        withContext(coroutineContext) {
            val creditCards = storage.value.getAllCreditCards()

            val foundCreditCard = if (creditCards.isEmpty()) {
                // No credit cards exist in the storage -> create a new credit card
                null
            } else {
                val crypto = storage.value.getCreditCardCrypto()
                val key = crypto.getOrGenerateKey()

                creditCards.find {
                    val cardNumber = crypto.decrypt(key, it.encryptedCardNumber)?.number

                    it.guid == creditCard.guid || cardNumber == creditCard.number
                }
            }

            if (foundCreditCard == null) {
                Result.CanBeCreated
            } else {
                Result.CanBeUpdated(foundCreditCard)
            }
        }
}
