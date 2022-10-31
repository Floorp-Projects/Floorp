/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.storage

/**
 * Knows how to provide a [ManagedKey].
 */
interface KeyProvider {
    /**
     * Fetches or generates a new encryption key.
     *
     * @return [ManagedKey] that wraps the encryption key.
     */
    suspend fun getOrGenerateKey(): ManagedKey
}

/**
 * An encryption key, with an optional [wasGenerated] field used to indicate if it was freshly
 * generated. In that case, a [KeyGenerationReason] is supplied, allowing consumers to detect
 * potential key loss or corruption.
 * If [wasGenerated] is `null`, that means an existing key was successfully read from the key storage.
 */
data class ManagedKey(
    val key: String,
    val wasGenerated: KeyGenerationReason? = null,
)

/**
 * Describes why a key was generated.
 */
sealed class KeyGenerationReason {
    /**
     * A new key, not previously present in the store.
     */
    object New : KeyGenerationReason()

    /**
     * Something went wrong with the previously stored key.
     */
    sealed class RecoveryNeeded : KeyGenerationReason() {
        /**
         * Previously stored key was lost, and a new key was generated as its replacement.
         */
        object Lost : RecoveryNeeded()

        /**
         * Previously stored key was corrupted, and a new key was generated as its replacement.
         */
        object Corrupt : RecoveryNeeded()

        /**
         * Storage layer encountered an abnormal state, which lead to key loss. A new key was generated.
         */
        object AbnormalState : RecoveryNeeded()
    }
}
