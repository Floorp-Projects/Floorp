/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.sync.autofill

import android.content.Context
import android.content.SharedPreferences
import mozilla.appservices.autofill.AutofillException
import mozilla.appservices.autofill.createKey
import mozilla.appservices.autofill.decryptString
import mozilla.appservices.autofill.encryptString
import mozilla.components.concept.storage.CreditCardCrypto
import mozilla.components.concept.storage.CreditCardNumber
import mozilla.components.concept.storage.KeyGenerationReason
import mozilla.components.concept.storage.KeyRecoveryHandler
import mozilla.components.concept.storage.ManagedKey
import mozilla.components.lib.dataprotect.SecureAbove22Preferences
import mozilla.components.support.base.log.logger.Logger

/**
 * A class that knows how to encrypt & decrypt strings, backed by application-services' autofill lib.
 * Used for protecting credit card numbers at rest.
 *
 * This class manages creation and storage of the encryption key.
 * It also keeps track of abnormal events, such as managed key going missing or getting corrupted.
 *
 * @param context [Context] used for obtaining [SharedPreferences] for managing internal prefs.
 * @param securePrefs A [SecureAbove22Preferences] instance used for storing the managed key.
 * @param keyRecoveryHandler A [KeyRecoveryHandler] instance that knows how to recover from key failures.
 */
class AutofillCrypto(
    private val context: Context,
    private val securePrefs: SecureAbove22Preferences,
    private val keyRecoveryHandler: KeyRecoveryHandler
) : CreditCardCrypto {
    private val logger = Logger("AutofillCrypto")
    private val plaintextPrefs by lazy { context.getSharedPreferences(AUTOFILL_PREFS, Context.MODE_PRIVATE) }

    override fun encrypt(
        key: ManagedKey,
        plaintextCardNumber: CreditCardNumber.Plaintext
    ): CreditCardNumber.Encrypted? {
        return try {
            CreditCardNumber.Encrypted(encrypt(key, plaintextCardNumber.number))
        } catch (e: AutofillException.JsonException) {
            logger.warn("Failed to encrypt", e)
            null
        } catch (e: AutofillException.CryptoException) {
            logger.warn("Failed to encrypt", e)
            null
        }
    }

    override fun decrypt(
        key: ManagedKey,
        encryptedCardNumber: CreditCardNumber.Encrypted
    ): CreditCardNumber.Plaintext? {
        return try {
            CreditCardNumber.Plaintext(decrypt(key, encryptedCardNumber.number))
        } catch (e: AutofillException.JsonException) {
            logger.warn("Failed to decrypt", e)
            null
        } catch (e: AutofillException.CryptoException) {
            logger.warn("Failed to decrypt", e)
            null
        }
    }

    override fun key(): ManagedKey = synchronized(this) {
        val managedKey = getManagedKey()

        // Record abnormal events if any were detected.
        // At this point, we should emit some telemetry.
        // See https://github.com/mozilla-mobile/android-components/issues/10122
        when (managedKey.wasGenerated) {
            is KeyGenerationReason.RecoveryNeeded.Lost -> {
                logger.warn("Key lost")
            }
            is KeyGenerationReason.RecoveryNeeded.Corrupt -> {
                logger.warn("Key corrupted")
            }
            is KeyGenerationReason.RecoveryNeeded.AbnormalState -> {
                logger.warn("Abnormal state while reading the key")
            }
            null, KeyGenerationReason.New -> {
                // All good! Got either a brand new key or read a valid key.
            }
        }
        (managedKey.wasGenerated as? KeyGenerationReason.RecoveryNeeded)?.let {
            keyRecoveryHandler.recoverFromBadKey(it)
        }
        return managedKey
    }

    private fun encrypt(key: ManagedKey, cleartext: String) = encryptString(key.key, cleartext)
    private fun decrypt(key: ManagedKey, ciphertext: String) = decryptString(key.key, ciphertext)

    @Suppress("ComplexMethod")
    private fun getManagedKey(): ManagedKey = synchronized(this) {
        val encryptedCanaryPhrase = plaintextPrefs.getString(CANARY_PHRASE_CIPHERTEXT_KEY, null)
        val storedKey = securePrefs.getString(AUTOFILL_KEY)

        return@synchronized when {
            // We expected the key to be present, and it is.
            storedKey != null && encryptedCanaryPhrase != null -> {
                // Make sure that the key is valid.
                try {
                    if (CANARY_PHRASE_PLAINTEXT == decryptString(storedKey, encryptedCanaryPhrase)) {
                        ManagedKey(storedKey)
                    } else {
                        // A bad key should trigger a CryptoException, but check this branch just in case.
                        ManagedKey(generateAndStoreKey(), KeyGenerationReason.RecoveryNeeded.Corrupt)
                    }
                } catch (e: AutofillException.JsonException) {
                    ManagedKey(generateAndStoreKey(), KeyGenerationReason.RecoveryNeeded.Corrupt)
                } catch (e: AutofillException.CryptoException) {
                    ManagedKey(generateAndStoreKey(), KeyGenerationReason.RecoveryNeeded.Corrupt)
                }
            }

            // The key is present, but we didn't expect it to be there.
            storedKey != null && encryptedCanaryPhrase == null -> {
                // This isn't expected to happen. We can't check this key's validity.
                ManagedKey(generateAndStoreKey(), KeyGenerationReason.RecoveryNeeded.AbnormalState)
            }

            // We expected the key to be present, but it's gone missing on us.
            storedKey == null && encryptedCanaryPhrase != null -> {
                // At this point, we're forced to generate a new key to recover and move forward.
                // However, that means that any data that was previously encrypted is now unreadable.
                ManagedKey(generateAndStoreKey(), KeyGenerationReason.RecoveryNeeded.Lost)
            }

            // We didn't expect the key to be present, and it's not.
            storedKey == null && encryptedCanaryPhrase == null -> {
                // Normal case when interacting with this class for the first time.
                ManagedKey(generateAndStoreKey(), KeyGenerationReason.New)
            }

            else -> throw AutofillCryptoException.IllegalState()
        }
    }

    private fun generateAndStoreKey(): String {
        return createKey().also { newKey ->
            // To consider: should this be a non-destructive operation, just in case?
            // e.g. if we thought we lost the key, but actually did not, that would let us recover data later on.
            // otherwise, if we mess up and override a perfectly good key, the data is gone for good.
            securePrefs.putString(AUTOFILL_KEY, newKey)
            // To detect key corruption or absence, use the newly generated key to encrypt a known string.
            // See isKeyValid below.
            plaintextPrefs
                .edit()
                .putString(CANARY_PHRASE_CIPHERTEXT_KEY, encryptString(newKey, CANARY_PHRASE_PLAINTEXT))
                .apply()
        }
    }

    companion object {
        const val AUTOFILL_PREFS = "autofillCrypto"
        const val AUTOFILL_KEY = "autofillKey"
        const val CANARY_PHRASE_CIPHERTEXT_KEY = "canaryPhrase"
        const val CANARY_PHRASE_PLAINTEXT = "a string for checking validity of the key"
    }
}
