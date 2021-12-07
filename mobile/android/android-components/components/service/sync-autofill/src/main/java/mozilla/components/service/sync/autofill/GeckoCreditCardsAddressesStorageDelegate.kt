/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.sync.autofill

import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import mozilla.components.concept.storage.Address
import mozilla.components.concept.storage.CreditCard
import mozilla.components.concept.storage.CreditCardNumber
import mozilla.components.concept.storage.CreditCardsAddressesStorage
import mozilla.components.concept.storage.CreditCardsAddressesStorageDelegate

/**
 * [CreditCardsAddressesStorageDelegate] implementation.
 *
 * @param storage The [CreditCardsAddressesStorage] used for looking up addresses and credit cards to autofill.
 * @param scope [CoroutineScope] for long running operations. Defaults to using the [Dispatchers.IO].
 * @param isCreditCardAutofillEnabled callback allowing to limit [storage] operations if autofill is disabled.
 */
class GeckoCreditCardsAddressesStorageDelegate(
    private val storage: Lazy<CreditCardsAddressesStorage>,
    private val scope: CoroutineScope = CoroutineScope(Dispatchers.IO),
    private val isCreditCardAutofillEnabled: () -> Boolean = { false }
) : CreditCardsAddressesStorageDelegate {

    override suspend fun decrypt(encryptedCardNumber: CreditCardNumber.Encrypted): CreditCardNumber.Plaintext? {
        val crypto = storage.value.getCreditCardCrypto()
        val key = crypto.getOrGenerateKey()
        return crypto.decrypt(key, encryptedCardNumber)
    }

    override fun onAddressesFetch(): Deferred<List<Address>> {
        return scope.async {
            storage.value.getAllAddresses()
        }
    }

    override fun onAddressSave(address: Address) {
        TODO("Not yet implemented")
    }

    override fun onCreditCardsFetch(): Deferred<List<CreditCard>> {
        if (isCreditCardAutofillEnabled().not()) {
            return CompletableDeferred(listOf())
        }

        return scope.async {
            storage.value.getAllCreditCards()
        }
    }

    override fun onCreditCardSave(creditCard: CreditCard) {
        TODO("Not yet implemented")
    }
}
