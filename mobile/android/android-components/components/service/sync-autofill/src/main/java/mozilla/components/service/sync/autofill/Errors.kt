/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.sync.autofill

/**
 * Unrecoverable errors related to [AutofillCreditCardsAddressesStorage].
 * Do not catch these.
 */
internal sealed class AutofillStorageException(reason: Exception? = null) : RuntimeException(reason) {
    /**
     * Thrown if an attempt was made to persist a plaintext version of a credit card number.
     */
    class TriedToPersistPlaintextCardNumber : AutofillStorageException()
}

/**
 * Unrecoverable errors related to [AutofillCrypto].
 * Do not catch these.
 */
internal sealed class AutofillCryptoException(cause: Exception? = null) : RuntimeException(cause) {
    /**
     * Thrown if [AutofillCrypto] encounters an unexpected, unrecoverable state.
     */
    class IllegalState : AutofillCryptoException()
}
