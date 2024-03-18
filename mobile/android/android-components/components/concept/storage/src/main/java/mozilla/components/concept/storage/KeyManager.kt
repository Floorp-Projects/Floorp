/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.storage

import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlin.IllegalStateException

/**
 * Knows how to manage (generate, store, validate) keys and recover from their loss.
 */
abstract class KeyManager : KeyProvider {
    // Exists to ensure that key generation/validation/recovery flow is synchronized.
    private val keyMutex = Mutex()

    /**
     * @return Generated key.
     */
    abstract fun createKey(): String

    /**
     * Determines if [rawKey] is still valid for a given [canary], or if recovery is necessary.
     * @return Optional [KeyGenerationReason.RecoveryNeeded] if recovery is necessary.
     */
    abstract fun isKeyRecoveryNeeded(rawKey: String, canary: String): KeyGenerationReason.RecoveryNeeded?

    /**
     * Returns a stored canary, if there's one. A canary is some known string encrypted with the managed key.
     * @return an optional, stored canary string.
     */
    abstract fun getStoredCanary(): String?

    /**
     * Returns a stored key, if there's one.
     */
    abstract fun getStoredKey(): String?

    /**
     * Stores [key]; using the key, generate and store a canary.
     */
    abstract fun storeKeyAndCanary(key: String)

    /**
     * Recover from key loss that happened due to [reason].
     * If this KeyManager wraps a storage layer, it should probably remove the now-unreadable data.
     */
    abstract suspend fun recoverFromKeyLoss(reason: KeyGenerationReason.RecoveryNeeded)

    override suspend fun getOrGenerateKey(): ManagedKey = keyMutex.withLock {
        val managedKey = getManagedKey()

        (managedKey.wasGenerated as? KeyGenerationReason.RecoveryNeeded)?.let {
            recoverFromKeyLoss(managedKey.wasGenerated)
        }
        return managedKey
    }

    /**
     * Access should be guarded by [keyMutex].
     */
    private fun getManagedKey(): ManagedKey {
        val storedCanaryPhrase = getStoredCanary()
        val storedKey = getStoredKey()

        return when {
            // We expected the key to be present, and it is.
            storedKey != null && storedCanaryPhrase != null -> {
                // Make sure that the key is valid.
                when (val recoveryReason = isKeyRecoveryNeeded(storedKey, storedCanaryPhrase)) {
                    is KeyGenerationReason -> ManagedKey(generateAndStoreKey(), recoveryReason)
                    null -> ManagedKey(storedKey)
                }
            }

            // The key is present, but we didn't expect it to be there.
            storedKey != null && storedCanaryPhrase == null -> {
                // This isn't expected to happen. We can't check this key's validity.
                ManagedKey(generateAndStoreKey(), KeyGenerationReason.RecoveryNeeded.AbnormalState)
            }

            // We expected the key to be present, but it's gone missing on us.
            storedKey == null && storedCanaryPhrase != null -> {
                // At this point, we're forced to generate a new key to recover and move forward.
                // However, that means that any data that was previously encrypted is now unreadable.
                ManagedKey(generateAndStoreKey(), KeyGenerationReason.RecoveryNeeded.Lost)
            }

            // We didn't expect the key to be present, and it's not.
            storedKey == null && storedCanaryPhrase == null -> {
                // Normal case when interacting with this class for the first time.
                ManagedKey(generateAndStoreKey(), KeyGenerationReason.New)
            }

            else -> throw IllegalStateException()
        }
    }

    /**
     * Access should be guarded by [keyMutex].
     */
    private fun generateAndStoreKey(): String {
        return createKey().also { newKey ->
            storeKeyAndCanary(newKey)
        }
    }
}
