/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.sync.logins

import android.content.Context
import android.content.SharedPreferences
import mozilla.appservices.logins.checkCanary
import mozilla.appservices.logins.createCanary
import mozilla.appservices.logins.createKey
import mozilla.appservices.logins.decryptFields
import mozilla.components.concept.storage.EncryptedLogin
import mozilla.components.concept.storage.KeyGenerationReason
import mozilla.components.concept.storage.KeyProvider
import mozilla.components.concept.storage.KeyRecoveryHandler
import mozilla.components.concept.storage.Login
import mozilla.components.concept.storage.ManagedKey
import mozilla.components.lib.dataprotect.SecureAbove22Preferences
import mozilla.components.support.base.log.logger.Logger

/**
 * A class that knows how to encrypt & decrypt strings, backed by application-services' logins lib.
 * Used for protecting usernames/passwords at rest.
 *
 * This class manages creation and storage of the encryption key.
 * It also keeps track of abnormal events, such as managed key going missing or getting corrupted.
 *
 * @param context [Context] used for obtaining [SharedPreferences] for managing internal prefs.
 * @param securePrefs A [SecureAbove22Preferences] instance used for storing the managed key.
 * @param keyRecoveryHandler A [KeyRecoveryHandler] instance that knows how to recover from key failures.
 */
class LoginsCrypto(
    private val context: Context,
    private val securePrefs: SecureAbove22Preferences,
    private val keyRecoveryHandler: KeyRecoveryHandler
) : KeyProvider {
    private val logger = Logger("LoginsCrypto")
    private val plaintextPrefs by lazy { context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE) }

    /**
     * Decrypts ciphertext fields within [login], producing a plaintext [Login].
     */
    fun decryptLogin(login: EncryptedLogin): Login {
        return decryptLogin(login, key())
    }

    /**
     * Decrypts ciphertext fields within [login], producing a plaintext [Login].
     *
     * This version inputs a ManagedKey.  Use this for operations that
     * decrypt multiple logins to avoid constructing the key multiple times.
     */
    fun decryptLogin(login: EncryptedLogin, key: ManagedKey): Login {
        val secFields = decryptFields(login.secFields, key.key)
        // Note: The autofill code catches errors on decryptFields and returns
        // null, but it's not as easy to recover in this case since the code
        // almost certainly going to need to a [Login], so we just throw in
        // that case.  Decryption errors shouldn't be happen as long as the
        // canary checking code below is working correctly

        return Login(
            guid = login.guid,
            origin = login.origin,
            username = secFields.username,
            password = secFields.password,
            formActionOrigin = login.formActionOrigin,
            httpRealm = login.httpRealm,
            usernameField = login.usernameField,
            passwordField = login.passwordField,
            timesUsed = login.timesUsed,
            timeCreated = login.timeCreated,
            timeLastUsed = login.timeLastUsed,
            timePasswordChanged = login.timePasswordChanged,
        )
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

    @Suppress("ComplexMethod")
    private fun getManagedKey(): ManagedKey = synchronized(this) {
        val encryptedCanaryPhrase = plaintextPrefs.getString(CANARY_PHRASE_CIPHERTEXT_KEY, null)
        val storedKey = securePrefs.getString(LOGINS_KEY)

        return@synchronized when {
            // We expected the key to be present, and it is.
            storedKey != null && encryptedCanaryPhrase != null -> {
                // Make sure that the key is valid.
                try {
                    if (checkCanary(encryptedCanaryPhrase, CANARY_PHRASE_PLAINTEXT, storedKey)) {
                        ManagedKey(storedKey)
                    } else {
                        // A bad key should trigger a CryptoException, but check this branch just in case.
                        ManagedKey(generateAndStoreKey(), KeyGenerationReason.RecoveryNeeded.Corrupt)
                    }
                } catch (e: CryptoException) {
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

            // The above cases seem exhaustive, but Kotlin doesn't think so.
            // Throw IllegalStateException if we get here.
            else -> throw IllegalStateException()
        }
    }

    private fun generateAndStoreKey(): String {
        return createKey().also { newKey ->
            // To consider: should this be a non-destructive operation, just in case?
            // e.g. if we thought we lost the key, but actually did not, that would let us recover data later on.
            // otherwise, if we mess up and override a perfectly good key, the data is gone for good.
            securePrefs.putString(LOGINS_KEY, newKey)
            // To detect key corruption or absence, use the newly generated key to encrypt a known string.
            // See isKeyValid below.
            plaintextPrefs
                .edit()
                .putString(CANARY_PHRASE_CIPHERTEXT_KEY, createCanary(CANARY_PHRASE_PLAINTEXT, newKey))
                .apply()
        }
    }

    companion object {
        const val PREFS_NAME = "loginsCrypto"
        const val LOGINS_KEY = "loginsKey"
        const val CANARY_PHRASE_CIPHERTEXT_KEY = "canaryPhrase"
        const val CANARY_PHRASE_PLAINTEXT = "a string for checking validity of the key"
    }
}
